/****************************************************************************
**
** Copyright (C) 2014 Alexander Rössler
** License: LGPL version 2.1
**
** This file is part of QtQuickVcp.
**
** All rights reserved. This program and the accompanying materials
** are made available under the terms of the GNU Lesser General Public License
** (LGPL) version 2.1 which accompanies this distribution, and is available at
** http://www.gnu.org/licenses/lgpl-2.1.html
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Lesser General Public License for more details.
**
** Contributors:
** Alexander Rössler @ The Cool Tool GmbH <mail DOT aroessler AT gmail DOT com>
**
****************************************************************************/
#ifndef QCOMPONENT_H
#define QCOMPONENT_H

#include <abstractserviceimplementation.h>
#include <QCoreApplication>
#include <QHash>
#include <QTimer>
#include <QUuid>
#include <nzmqt/nzmqt.hpp>
#include <machinetalk/protobuf/message.pb.h>
#include <google/protobuf/text_format.h>
#include "qhalpin.h"
#include "machinetalkrpcclient.h"
#include "machinetalksubscriber.h"

#if defined(Q_OS_IOS)
namespace gpb = google_public::protobuf;
#else
namespace gpb = google::protobuf;
#endif

using namespace nzmqt;

class QHalRemoteComponent : public AbstractServiceImplementation
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QString halrcmdUri READ halrcmdUri WRITE setHalrcmdUri NOTIFY halrcmdUriChanged)
    Q_PROPERTY(QString halrcompUri READ halrcompUri WRITE setHalrcompUri NOTIFY halrcompUriChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(int heartbeatPeriod READ heartbeatPeriod WRITE heartbeatPeriod NOTIFY heartbeatPeriodChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(State connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(ConnectionError error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)
    Q_PROPERTY(QObject *containerItem READ containerItem WRITE setContainerItem NOTIFY containerItemChanged)
    Q_PROPERTY(bool create READ create WRITE setCreate NOTIFY createChanged)
    Q_ENUMS(SocketState)
    Q_ENUMS(State)
    Q_ENUMS(ConnectionError)

public:
    explicit QHalRemoteComponent(QObject *parent = 0);

    enum State {
        Disconnected = 0,
        Connecting = 1,
        Connected = 2,
        Timeout = 3,
        Error = 4
    };

    enum ConnectionError {
        NoError = 0,
        BindError = 1,
        PinChangeError = 2,
        CommandError = 3,
        SocketError = 4
    };

    QString halrcmdUri() const
    {
        return m_rpcClient->uri();
    }

    QString halrcompUri() const
    {
        return m_subscriber->uri();
    }

    QString name() const
    {
        return m_name;
    }

    int heartbeatPeriod() const
    {
        return m_rpcClient->heartbeatPeriod();
    }

    QObject *containerItem() const
    {
        return m_containerItem;
    }

    State connectionState() const
    {
        return m_connectionState;
    }

    ConnectionError error() const
    {
        return m_error;
    }

    QString errorString() const
    {
        return m_errorString;
    }

    bool isConnected() const
    {
        return m_connected;
    }

    bool create() const
    {
        return m_create;
    }

public slots:
    void pinChange(QVariant value);

    void setHalrcmdUri(QString arg)
    {
        m_rpcClient->setUri(arg);
    }

    void setHalrcompUri(QString arg)
    {
        m_subscriber->setUri(arg);
    }

    void setName(QString arg)
    {
        if (m_connectionState != Disconnected)
            return;

        if (m_name != arg) {
            m_name = arg;
            emit nameChanged(arg);
        }
    }

    void heartbeatPeriod(int arg)
    {
        m_rpcClient->setHeartbeatPeriod(arg);
    }

    void setContainerItem(QObject *arg)
    {
        if (m_containerItem != arg) {
            m_containerItem = arg;
            emit containerItemChanged(arg);
        }
    }

    void setCreate(bool arg)
    {
        if (m_create == arg)
            return;

        m_create = arg;
        emit createChanged(arg);
    }

private:
    QString     m_name;
    bool        m_connected;
    State       m_connectionState;
    ConnectionError       m_error;
    QString     m_errorString;
    QObject     *m_containerItem;
    bool        m_create;

    MachinetalkRpcClient  *m_rpcClient;
    MachinetalkSubscriber *m_subscriber;
    // more efficient to reuse a protobuf Message
    pb::Container   m_tx;
    QMap<QString, QHalPin*> m_pinsByName;
    QHash<int, QHalPin*>    m_pinsByHandle;


    QObjectList recurseObjects(const QObjectList &list);
    void start();
    void stop();
    void cleanup();
    void updateState(State state);
    void updateState(State state, ConnectionError error, QString errorString);
    void updateError(ConnectionError error, QString errorString);

private slots:
    void pinUpdate(const pb::Pin &remotePin, QHalPin *localPin);

    void halrcompMessageReceived(QByteArray topic, pb::Container *rx);
    void halrcmdMessageReceived(pb::Container *rx);
    void socketStateChanged(SocketState state);
    void halrcmdStateChanged(SocketState state);
    void halrcompStateChanged(SocketState state);

    void addPins();
    void removePins();
    void unsyncPins();
    void bind();

signals:
    void halrcmdUriChanged(QString arg);
    void halrcompUriChanged(QString arg);
    void nameChanged(QString arg);
    void heartbeatPeriodChanged(int arg);
    void containerItemChanged(QObject *arg);
    void connectionStateChanged(State arg);
    void errorChanged(ConnectionError arg);
    void errorStringChanged(QString arg);
    void connectedChanged(bool arg);
    void createChanged(bool arg);
};

#endif // QCOMPONENT_H
