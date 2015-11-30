#include "machinetalkrpcclient.h"
#include "debughelper.h"

/** Generic Machinetalk RPC client implementation */
MachinetalkRpcClient::MachinetalkRpcClient(QObject *parent) :
    QObject(parent),
    m_ready(false),
    m_uri(""),
    m_debugName(""),
    m_context(NULL),
    m_socket(NULL),
    m_socketState(SocketDown),
    m_errorString(""),
    m_heartbeatTimer(new QTimer(this)),
    m_heartbeatPeriod(3000),
    m_pingErrorCount(0),
    m_pingErrorThreshold(2)
{
    m_uuid = QUuid::createUuid();

    connect(m_heartbeatTimer, SIGNAL(timeout()),
            this, SLOT(heartbeatTimerTick()));
}

MachinetalkRpcClient::~MachinetalkRpcClient()
{
    stop();
}

/** Connects the 0MQ sockets */
bool MachinetalkRpcClient::connectSockets()
{
    m_context = new SocketNotifierZMQContext(this, 1);
    connect(m_context, SIGNAL(notifierError(int,QString)),
            this, SLOT(socketError(int,QString)));
    m_context->start();

    m_socket = m_context->createSocket(ZMQSocket::TYP_DEALER, this);
    m_socket->setLinger(0);
    m_socket->setIdentity(QString("%1-%2").arg(QHostInfo::localHostName()).arg(m_uuid.toString()).toLocal8Bit());

    try {
        m_socket->connectTo(m_uri);
    }
    catch (const zmq::error_t &e) {
        QString errorString;
        errorString = QString("Error %1: ").arg(e.num()) + QString(e.what());
        updateState(SocketError, errorString);
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
void MachinetalkRpcClient::disconnectSockets()
{
    updateState(SocketDown);

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

void MachinetalkRpcClient::start()
{
#ifdef QT_DEBUG
   DEBUG_TAG(1, m_debugName, "start");
#endif

    updateState(SocketTrying);

    if (connectSockets())
    {
        m_pingErrorCount = 0;  // reset the error count
        refreshHeartbeat();  // start the heartbeat
        sendMessage(pb::MT_PING, &m_tx);
    }
}

void MachinetalkRpcClient::stop()
{
#ifdef QT_DEBUG
    DEBUG_TAG(1, m_debugName, "stop");
#endif

    stopHeartbeat();
    disconnectSockets();
}

void MachinetalkRpcClient::refreshHeartbeat()
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

void MachinetalkRpcClient::stopHeartbeat()
{
    m_heartbeatTimer->stop();
}

void MachinetalkRpcClient::updateState(SocketState state)
{
    updateState(state, "");
}

void MachinetalkRpcClient::updateState(SocketState state, QString errorString)
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
    states.insert(SocketDown, "DOWN");
    states.insert(SocketTrying, "TRYING");
    states.insert(SocketUp, "UP");
    states.insert(SocketTimeout, "TIMEOUT");
    states.insert(SocketError, "ERROR");
    DEBUG_TAG(1, m_debugName, states.value(m_socketState));
#endif
    }
}

void MachinetalkRpcClient::heartbeatTimerTick()
{
    sendMessage(pb::MT_PING, &m_tx);
    m_pingErrorCount += 1;

    if ((m_pingErrorCount > m_pingErrorThreshold) && (m_socketState == SocketUp))
    {
        updateState(SocketTimeout);
    }
}

/** Processes all message received on the 0MQ socket */
void MachinetalkRpcClient::socketMessageReceived(QList<QByteArray> messageList)
{
    m_rx.ParseFromArray(messageList.at(0).data(), messageList.at(0).size());

#ifdef QT_DEBUG
    std::string s;
    gpb::TextFormat::PrintToString(m_rx, &s);
    DEBUG_TAG(3, m_debugName, "server message" << QString::fromStdString(s));
#endif

    m_pingErrorCount = 0;  // any message counts as heartbeat since messages can be queued
    updateState(SocketUp);

    if (m_rx.type() != pb::MT_PING_ACKNOWLEDGE)  // ping acknowlege is uninteresting
    {
        emit messageReceived(&m_rx);
    }
}

void MachinetalkRpcClient::sendMessage(pb::ContainerType type, pb::Container *tx)
{
    if (m_socket == NULL) {  // disallow sending messages when not connected
        return;
    }

    try {
        tx->set_type(type);
        m_socket->sendMessage(QByteArray(tx->SerializeAsString().c_str(), tx->ByteSize()));
        tx->Clear();
    }
    catch (const zmq::error_t &e) {
        QString errorString;
        errorString = QString("Error %1: ").arg(e.num()) + QString(e.what());
        updateState(SocketError, errorString);
    }

    if (type == pb::MT_PING)
    {
        refreshHeartbeat();
    }
}

void MachinetalkRpcClient::socketError(int errorNum, const QString &errorMsg)
{
    QString errorString;
    errorString = QString("Error %1: ").arg(errorNum) + errorMsg;
    updateState(SocketError, errorString);
}
