#include "machinetalksubscriber.h"
#include "debughelper.h"

/** A generic Machinetalk subscriber socket **/
MachinetalkSubscriber::MachinetalkSubscriber(QObject *parent) :
    QObject(parent),
    m_uri(""),
    m_debugName(""),
    m_context(NULL),
    m_socket(NULL),
    m_socketState(Down),
    m_errorString(""),
    m_heartbeatPeriod(3000),
    m_heartbeatTimer(new QTimer(this))
{
    connect(m_heartbeatTimer, SIGNAL(timeout()),
            this, SLOT(heartbeatTimerTick()));
}

MachinetalkSubscriber::~MachinetalkSubscriber()
{
    stop();
}

/** Add a topic that should be subscribed **/
void MachinetalkSubscriber::addTopic(const QString &name)
{
    m_topics.insert(name);
}

/** Removes a topic from the list of topics that should be subscribed **/
void MachinetalkSubscriber::removeTopic(const QString &name)
{
    m_topics.remove(name);
}

/** Clears the the topics that should be subscribed **/
void MachinetalkSubscriber::clearTopics()
{
    m_topics.clear();
}

/** Connects the 0MQ sockets */
bool MachinetalkSubscriber::connectSockets()
{
    m_context = new PollingZMQContext(this, 1);
    connect(m_context, SIGNAL(pollError(int,QString)),
            this, SLOT(pollError(int,QString)));
    m_context->start();

    m_socket = m_context->createSocket(ZMQSocket::TYP_SUB, this);
    m_socket->setLinger(0);

    try {
        m_socket->connectTo(m_uri);
    }
    catch (const zmq::error_t &e) {
        QString errorString;
        errorString = QString("Error %1: ").arg(e.num()) + QString(e.what());
        updateState(Error, errorString);
        return false;
    }

    connect(m_socket, SIGNAL(messageReceived(QList<QByteArray>)),
            this, SLOT(socketMessageReceived(QList<QByteArray>)));

#ifdef QT_DEBUG
    DEBUG_TAG(1, m_debugName, "sockets connected" << m_uri);
#endif

    return true;
}

/** Disconnects the 0MQ sockets */
void MachinetalkSubscriber::disconnectSockets()
{
    updateState(Down);

    if (m_socket != NULL)
    {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = NULL;
    }

    if (m_context != NULL)
    {
        m_context->stop();
        m_context->deleteLater();
        m_context = NULL;
    }
}

void MachinetalkSubscriber::subscribe()
{
    updateState(Trying);
    m_heartbeatPeriod = 0;  // reset heartbeat
    foreach(QString topic, m_topics)
    {
        m_socket->subscribeTo(topic.toLocal8Bit());
        m_subscriptions.insert(topic);
    }
}

void MachinetalkSubscriber::unsubscribe()
{
    updateState(Down);
    foreach(QString topic, m_subscriptions)
    {
        m_socket->subscribeTo(topic.toLocal8Bit());
    }
    m_subscriptions.clear();
}

void MachinetalkSubscriber::start()
{
#ifdef QT_DEBUG
   DEBUG_TAG(1, m_debugName, "start");
#endif

   if (connectSockets())
   {
       subscribe();
   }
}

void MachinetalkSubscriber::stop()
{
#ifdef QT_DEBUG
    DEBUG_TAG(1, m_debugName, "stop");
#endif

    stopHeartbeat();
    disconnectSockets();
}

void MachinetalkSubscriber::refreshHeartbeat()
{
    if (m_heartbeatTimer->isActive())
    {
        m_heartbeatTimer->stop();
    }

    if (m_heartbeatPeriod > 0)
    {
        m_heartbeatTimer->setInterval(m_heartbeatPeriod);
        m_heartbeatTimer->start();
    }
}

void MachinetalkSubscriber::stopHeartbeat()
{
    m_heartbeatTimer->stop();
}

void MachinetalkSubscriber::updateState(MachinetalkSubscriber::SocketState state)
{
    updateState(state, "");
}

void MachinetalkSubscriber::updateState(MachinetalkSubscriber::SocketState state, QString errorString)
{
    if (state != m_socketState) {
        m_socketState = state;
        emit socketStateChanged(m_socketState);

        if (errorString != m_errorString)
        {
            m_errorString = errorString;
            emit errorStringChanged("");
        }

#ifdef QT_DEBUG
    QMap<SocketState, QString> states;
    states.insert(Down, "DOWN");
    states.insert(Trying, "TRYING");
    states.insert(Up, "UP");
    states.insert(Timeout, "TIMEOUT");
    states.insert(Error, "ERROR");
    DEBUG_TAG(1, m_debugName, states.value(m_socketState));
#endif
    }
}

void MachinetalkSubscriber::heartbeatTimerTick()
{
    updateState(Timeout);
    m_heartbeatTimer->stop();  // not needed anymore

#ifdef QT_DEBUG
    DEBUG_TAG(1, m_debugName, "timeout");
#endif
}

void MachinetalkSubscriber::socketMessageReceived(QList<QByteArray> messageList)
{
    QByteArray topic;

    if (messageList.length() < 2)  // in case we received insufficient data
    {
        return;
    }

    // we only handle the first two messges
    topic = messageList.at(0);  // TODO: we never check the topic
    m_rx.ParseFromArray(messageList.at(1).data(), messageList.at(1).size());

#ifdef QT_DEBUG
    std::string s;
    gpb::TextFormat::PrintToString(m_rx, &s);
    DEBUG_TAG(3, m_debugName, "status update" << topic << QString::fromStdString(s));
#endif

    if (m_rx.type() == pb::MT_HALRCOMP_FULL_UPDATE)
    {
        updateState(Up);

        if (m_rx.has_pparams())
        {
            pb::ProtocolParameters pparams = m_rx.pparams();
            m_heartbeatPeriod = pparams.keepalive_timer() * 2; // wait double the time of the hearbeat interval
        }
    }

    if (m_socketState == Up)
    {
        refreshHeartbeat();  // refresh heartbeat if any message is received
        if (m_rx.type() != pb::MT_PING)  // pings are uninteresting
        {
            emit messageReceived(topic, &m_rx);
        }
    }
    else
    {
        unsubscribe();  // clean up previous subscription
        subscribe();  // trigger a fresh subscribe -> full update
    }
}

void MachinetalkSubscriber::pollError(int errorNum, const QString &errorMsg)
{
    QString errorString;
    errorString = QString("Error %1: ").arg(errorNum) + errorMsg;
    updateState(Error, errorString);
}
