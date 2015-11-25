#ifndef MACHINETALKSUBSCRIBER_H
#define MACHINETALKSUBSCRIBER_H

#include <QObject>
#include <QSet>
#include <nzmqt/nzmqt.hpp>
#include "message.pb.h"
#include <google/protobuf/text_format.h>


#if defined(Q_OS_IOS)
namespace gpb = google_public::protobuf;
#else
namespace gpb = google::protobuf;
#endif

using namespace nzmqt;

class MachinetalkSubscriber : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString uri READ uri WRITE setUri NOTIFY uriChanged)
    Q_PROPERTY(QString debugName READ debugName WRITE setDebugName NOTIFY debugNameChanged)
    Q_PROPERTY(SocketState socketState READ socketState NOTIFY socketStateChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_ENUMS(SocketState)

public:
    explicit MachinetalkSubscriber(QObject *parent = 0);
    ~MachinetalkSubscriber();

    enum SocketState {
        Down = 1,
        Trying = 2,
        Up = 3,
        Timeout = 4,
        Error = 5
    };

    QString uri() const
    {
        return m_uri;
    }

    QString debugName() const
    {
        return m_debugName;
    }

    SocketState socketState() const
    {
        return m_socketState;
    }

    QString errorString() const
    {
        return m_errorString;
    }

public slots:

    void setUri(QString uri)
    {
        if (m_uri == uri)
            return;

        m_uri = uri;
        emit uriChanged(uri);
    }

    void setDebugName(QString debugName)
    {
        if (m_debugName == debugName)
            return;

        m_debugName = debugName;
        emit debugNameChanged(debugName);
    }

    void addTopic(const QString &name);
    void removeTopic(const QString &name);
    void clearTopics();

private:

    QString m_uri;
    QString m_debugName;
    QSet<QString> m_topics;  // the topics we are interested in
    QSet<QString> m_subscriptions;  // subscribed topics
    QSet<QString> m_publishers;  // connected publishers

    PollingZMQContext *m_context;
    ZMQSocket  *m_socket;
    SocketState m_socketState;
    QString     m_errorString;
    int         m_heartbeatPeriod;
    QTimer     *m_heartbeatTimer;
    pb::Container   m_rx;  // more efficient to reuse a protobuf Message

    void start();
    void stop();
    void cleanup();
    void refreshHeartbeat();
    void stopHeartbeat();
    void updateState(SocketState state);
    void updateState(SocketState state, QString errorString);

private slots:
    void heartbeatTimerTick();
    void socketMessageReceived(QList<QByteArray> messageList);
    void pollError(int errorNum, const QString& errorMsg);

    bool connectSockets();
    void disconnectSockets();
    void subscribe();
    void unsubscribe();

signals:

    void uriChanged(QString uri);
    void debugNameChanged(QString debugName);
    void messageReceived(QByteArray topic, pb::Container *rx);
    void socketStateChanged(SocketState socketState);
    void errorStringChanged(QString errorString);
};

#endif // MACHINETALKSUBSCRIBER_H
