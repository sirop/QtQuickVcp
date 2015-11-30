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
#include "qhalremotecomponent.h"
#include "debughelper.h"

/*!
    \qmltype HalRemoteComponent
    \instantiates QHalRemoteComponent
    \inqmlmodule Machinekit.HalRemote
    \brief A HAL remote component.
    \ingroup halremote

    This component provides the counterpart of a HAL
    remote component in the HAL real-time environment.
    The HalRemoteComponent connects to a remote HAL instance
    using the halrcmd and halrcomp services provided by
    a Haltalk instance running on the remote host.

    A HalRemoteComponent needs the \l{halrcmdUri},
    \l{halrcompUri} and \l containerItem set in order
    to work.

    The HalRemoteComponent scans the \l containerItem
    and its children for \l{HalPin}s when \l ready is set
    to \c true.

    The following example creates a HAL remote component
    \c myComponent with one pin \c myPin. The resulting
    name for the pin inside HAL is \c myComponent.myPin.

    \qml
    Item {
        Item {
            id: container

            HalPin {
                name: "myPin"
                type: HalPin.Float
                direction: HalPin.Out
            }
        }

        HalRemoteComponent {
            id: halRemoteComponent

            name: "myComponent"
            halrcmdUri: "tcp://192.168.1.2:5001"
            halrcompUri: "tcp://192.168.1.2:5002"
            containerItem: container
            ready: true
        }
    }
    \endqml
*/

/*! \qmlproperty string HalRemoteComponent::halrcmdUri

    This property holds the halrcmd service uri.
*/

/*! \qmlproperty string HalRemoteComponent::halrcompUri

    This property holds the halrcomp service uri.
*/

/*! \qmlproperty string HalRemoteComponent::name

    This property holds the name of the remote component.
*/

/*! \qmlproperty int HalRemoteComponent::heartbeadPeriod

    This property holds the period time of the heartbeat timer in ms.
    Set this property to \c{0} to disable the hearbeat.

    The default value is \c{3000}.
*/

/*! \qmlproperty bool HalRemoteComponent::ready

    This property holds whether the HalRemoteComponent is ready or not.
    If the property is set to \c true the component will try to connect. If the
    property is set to \c false all connections will be closed.

    The default value is \c{false}.
*/

/*! \qmlproperty bool HalRemoteComponent::connected

    This property holds wheter the HAL remote component is connected or not. This is the
    same as \l{connectionState} == \c{HalRemoteComponent.Connected}.
 */

/*! \qmlproperty enumeration HalRemoteComponent::connectionState

    This property holds the connection state of the HAL remote component.

    \list
    \li HalRemoteComponent.Disconnected - The component is not connected.
    \li HalRemoteComponent.Connecting - The component is trying to connect to the remote component.
    \li HalRemoteComponent.Connected - The component is connected and pin changes are accepted.
    \li HalRemoteComponent.Error - An error has happened. See \l error and \l errorString for details about the error.
    \endlist
*/

/*! \qmlproperty enumeration HalRemoteComponent::error

    This property holds the currently active error. See \l errorString
    for a description of the active error.

    \list
    \li HalRemoteComponent.NoError - No error happened.
    \li HalRemoteComponent.BindError - Binding the remote component failed.
    \li HalRemoteComponent.PinChangeError - A pin change was rejected.
    \li HalRemoteComponent.CommandError - A command was rejected.
    \li HalRemoteComponent.TimeoutError - The connection timed out.
    \li HalRemoteComponent.SocketError - An error related to the sockets happened.
    \endlist

    \sa errorString
*/

/*! \qmlproperty string HalRemoteComponent::errorString

    This property holds a text description of the last error that occured.
    If \l error holds a error value check this property for the description.

    \sa error
*/

/*! \qmlproperty Item HalRemoteComponent::containerItem

    This property holds the item that should be scanned for
    \l{HalPin}s.

    The default value is \c{NULL}.
*/

/*! \qmlproperty Item HalRemoteComponent::create

    Specifies wether the component should be created on bind if it
    does not exist on the remote host.

    The default value is \c{true}.
*/

/** Remote HAL Component implementation for use with C++ and QML */
QHalRemoteComponent::QHalRemoteComponent(QObject *parent) :
    AbstractServiceImplementation(parent),
    m_name("default"),
    m_connected(false),
    m_connectionState(Disconnected),
    m_error(NoError),
    m_errorString(""),
    m_containerItem(this),
    m_create(true)
{
    m_rpcClient = new MachinetalkRpcClient(this);
    connect(m_rpcClient, SIGNAL(heartbeatPeriodChanged(int)),
            this, SIGNAL(heartbeatPeriodChanged(int)));
    connect(m_rpcClient, SIGNAL(uriChanged(QString)),
            this, SIGNAL(halrcmdUriChanged(QString)));
    connect(m_rpcClient, SIGNAL(messageReceived(pb::Container*)),
            this, SLOT(halrcmdMessageReceived(pb::Container*)));
    connect(m_rpcClient, SIGNAL(socketStateChanged(SocketState)),
            this, SLOT(socketStateChanged(SocketState)));
    connect(m_rpcClient, SIGNAL(socketStateChanged(SocketState)),
            this, SLOT(halrcmdStateChanged(SocketState)));
    m_subscriber = new MachinetalkSubscriber(this);
    connect(m_subscriber, SIGNAL(uriChanged(QString)),
            this, SIGNAL(halrcompUriChanged(QString)));
    connect(m_subscriber, SIGNAL(messageReceived(QByteArray,pb::Container*)),
            this, SLOT(halrcompMessageReceived(QByteArray,pb::Container*)));
    connect(m_subscriber, SIGNAL(socketStateChanged(SocketState)),
            this, SLOT(socketStateChanged(SocketState)));
}

/** Scans all children of the container item for pins and adds them to a map */
void QHalRemoteComponent::addPins()
{
    QObjectList halObjects;

    if (m_containerItem == NULL)
    {
        return;
    }

    halObjects = recurseObjects(m_containerItem->children());
    foreach (QObject *object, halObjects)
    {
        QHalPin *pin = static_cast<QHalPin *>(object);
        if (pin->name().isEmpty()  || (pin->enabled() == false))    // ignore pins with empty name and disabled pins
        {
            continue;
        }
        m_pinsByName[pin->name()] = pin;
        connect(pin, SIGNAL(valueChanged(QVariant)),
                this, SLOT(pinChange(QVariant)));
#ifdef QT_DEBUG
        DEBUG_TAG(1, m_name, "pin added: " << pin->name())
#endif
    }
}

/** Removes all previously added pins */
void QHalRemoteComponent::removePins()
{
    foreach (QHalPin *pin, m_pinsByName)
    {
        disconnect(pin, SIGNAL(valueChanged(QVariant)),
                this, SLOT(pinChange(QVariant)));
    }

    m_pinsByHandle.clear();
    m_pinsByName.clear();
}

/** Sets synced of all pins to false */
void QHalRemoteComponent::unsyncPins()
{
    QMapIterator<QString, QHalPin*> i(m_pinsByName);
    while (i.hasNext()) {
        i.next();
        i.value()->setSynced(false);
    }
}

/** Generates a Bind messages and sends it over the suitable 0MQ socket */
void QHalRemoteComponent::bind()
{
    pb::Component *component;

    component = m_tx.add_comp();
    component->set_name(m_name.toStdString());
    component->set_no_create(!m_create);
    foreach (QHalPin *pin, m_pinsByName)
    {
        pb::Pin *halPin = component->add_pin();
        halPin->set_name(QString("%1.%2").arg(m_name).arg(pin->name()).toStdString());  // pin name is always component.name
        halPin->set_type((pb::ValueType)pin->type());
        halPin->set_dir((pb::HalPinDirection)pin->direction());
        if (pin->type() == QHalPin::Float)
        {
            halPin->set_halfloat(pin->value().toDouble());
        }
        else if (pin->type() == QHalPin::Bit)
        {
            halPin->set_halbit(pin->value().toBool());
        }
        else if (pin->type() == QHalPin::S32)
        {
            halPin->set_hals32(pin->value().toInt());
        }
        else if (pin->type() == QHalPin::U32)
        {
            halPin->set_halu32(pin->value().toUInt());
        }
    }

#ifdef QT_DEBUG
    std::string s;
    gpb::TextFormat::PrintToString(m_tx, &s);
    DEBUG_TAG(1, m_name, "bind")
    DEBUG_TAG(3, m_name, QString::fromStdString(s))
#endif

    m_rpcClient->sendMessage(pb::MT_HALRCOMP_BIND, &m_tx);
}

/** Updates a local pin with the value of a remote pin */
void QHalRemoteComponent::pinUpdate(const pb::Pin &remotePin, QHalPin *localPin)
{
#ifdef QT_DEBUG
    DEBUG_TAG(2, m_name,  "pin update" << localPin->name() << remotePin.halfloat() << remotePin.halbit() << remotePin.hals32() << remotePin.halu32())
#endif

    if (remotePin.has_halfloat())
    {
        localPin->setValue(QVariant(remotePin.halfloat()), true);
    }
    else if (remotePin.has_halbit())
    {
        localPin->setValue(QVariant(remotePin.halbit()), true);
    }
    else if (remotePin.has_hals32())
    {
        localPin->setValue(QVariant(remotePin.hals32()), true);
    }
    else if (remotePin.has_halu32())
    {
        localPin->setValue(QVariant(remotePin.halu32()), true);
    }
}

/** Updates a remote pin witht the value of a local pin */
void QHalRemoteComponent::pinChange(QVariant value)
{
    Q_UNUSED(value)
    QHalPin *pin;
    pb::Pin *halPin;

    if (m_connectionState != Connected) // only accept pin changes if we are connected
    {
        return;
    }

    pin = static_cast<QHalPin *>(QObject::sender());

    if (pin->direction() == QHalPin::In)   // Only update Output or IO pins
    {
        return;
    }

#ifdef QT_DEBUG
    DEBUG_TAG(2, m_name,  "pin change" << pin->name() << pin->value())
#endif

    // This message MUST carry a Pin message for each pin which has
    // changed value since the last message of this type.
    // Each Pin message MUST carry the handle field.
    // Each Pin message MAY carry the name field.
    // Each Pin message MUST carry the type field
    // Each Pin message MUST - depending on pin type - carry a halbit,
    // halfloat, hals32, or halu32 field.
    halPin = m_tx.add_pin();

    halPin->set_handle(pin->handle());
    halPin->set_type((pb::ValueType)pin->type());
    if (pin->type() == QHalPin::Float)
    {
        halPin->set_halfloat(pin->value().toDouble());
    }
    else if (pin->type() == QHalPin::Bit)
    {
        halPin->set_halbit(pin->value().toBool());
    }
    else if (pin->type() == QHalPin::S32)
    {
        halPin->set_hals32(pin->value().toInt());
    }
    else if (pin->type() == QHalPin::U32)
    {
        halPin->set_halu32(pin->value().toUInt());
    }

    m_rpcClient->sendMessage(pb::MT_HALRCOMP_SET, &m_tx);
}

void QHalRemoteComponent::start()
{
#ifdef QT_DEBUG
   DEBUG_TAG(1, m_name, "start")
#endif
    //updateState(Connecting);

    addPins();
    m_subscriber->clearTopics();  // set the subscription topic => component name
    m_subscriber->addTopic(m_name);
    m_rpcClient->setReady(true);
}

void QHalRemoteComponent::stop()
{
#ifdef QT_DEBUG
    DEBUG_TAG(1, m_name, "stop")
#endif

    cleanup();

    //updateState(Disconnected);  // clears also the error
}

void QHalRemoteComponent::cleanup()
{
    m_subscriber->setReady(false);
    m_rpcClient->setReady(false);
    removePins();
}

void QHalRemoteComponent::updateState(QHalRemoteComponent::State state)
{
    updateState(state, NoError, "");
}

void QHalRemoteComponent::updateState(State state, QHalRemoteComponent::ConnectionError error, QString errorString)
{
    if (state != m_connectionState)
    {
        if (m_connectionState == Connected) // we are not connected anymore
        {
            unsyncPins();
        }

        m_connectionState = state;
        emit connectionStateChanged(m_connectionState);

        if (m_connectionState == Connected)
        {
            if (m_connected != true) {
                m_connected = true;
                emit connectedChanged(true);
            }
        }
        else
        {
            if (m_connected != false) {
                m_connected = false;
                emit connectedChanged(false);
            }
        }
    }

    updateError(error, errorString);
}

void QHalRemoteComponent::updateError(QHalRemoteComponent::ConnectionError error, QString errorString)
{
    if (m_errorString != errorString)
    {
        m_errorString = errorString;
        emit errorStringChanged(m_errorString);
    }

    if (m_error != error)
    {
        if (error != NoError)
        {
            cleanup();
        }
        m_error = error;
        emit errorChanged(m_error);
    }
}

/** Recurses through a list of objects */
QObjectList QHalRemoteComponent::recurseObjects(const QObjectList &list)
{
    QObjectList halObjects;

    foreach (QObject *object, list)
    {
        QHalPin *halPin;
        halPin = qobject_cast<QHalPin *>(object);
        if (halPin != NULL)
        {
            halObjects.append(object);
        }

        if (object->children().size() > 0)
        {
            halObjects.append(recurseObjects(object->children()));
        }
    }

    return halObjects;
}

/** Processes all message received on the update 0MQ socket */
void QHalRemoteComponent::halrcompMessageReceived(QByteArray topic, pb::Container *rx)
{
#ifdef QT_DEBUG
    std::string s;
    gpb::TextFormat::PrintToString(*rx, &s);
    DEBUG_TAG(3, m_name, "status update" << topic << QString::fromStdString(s))
#endif

    if (rx->type() == pb::MT_HALRCOMP_INCREMENTAL_UPDATE) //incremental update
    {
#ifdef QT_DEBUG
        DEBUG_TAG(1, m_name, "incremental update")
#endif
        for (int i = 0; i < rx->pin_size(); ++i)
        {
            pb::Pin remotePin = rx->pin(i);
            QHalPin *localPin = m_pinsByHandle.value(remotePin.handle(), NULL);
            if (localPin != NULL) // in case we received a wrong pin handle
            {
                pinUpdate(remotePin, localPin);
            }
        }

        return;
    }
    else if (rx->type() == pb::MT_HALRCOMP_FULL_UPDATE)
    {
#ifdef QT_DEBUG
        DEBUG_TAG(1, m_name, "full update")
#endif
        for (int i = 0; i < rx->comp_size(); ++i)  // TODO: we can only handle one component
        {
            pb::Component component = rx->comp(i);  // shouldnt we check the name?
            for (int j = 0; j < component.pin_size(); j++)
            {
                pb::Pin remotePin = component.pin(j);
                QString name = QString::fromStdString(remotePin.name());
                int dotIndex = name.indexOf(".");
                if (dotIndex != -1)    // strip comp prefix
                {
                    name = name.mid(dotIndex + 1);
                }
                QHalPin *localPin = m_pinsByName.value(name);
                localPin->setHandle(remotePin.handle());
                m_pinsByHandle.insert(remotePin.handle(), localPin);
                pinUpdate(remotePin, localPin);
            }
        }

        return;
    }
    else if (rx->type() == pb::MT_HALRCOMMAND_ERROR)
    {
        QString errorString;

        for (int i = 0; i < rx->note_size(); ++i)
        {
            errorString.append(QString::fromStdString(rx->note(i)) + "\n");
        }

        updateState(Error, CommandError, errorString);

#ifdef QT_DEBUG
        DEBUG_TAG(1, m_name, "proto error on subscribe" << errorString)
#endif

        return;
    }

#ifdef QT_DEBUG
    gpb::TextFormat::PrintToString(*rx, &s);
    DEBUG_TAG(1, m_name, "status_update: unknown message type: " << QString::fromStdString(s))
#endif
}

/** Processes all message received on the command 0MQ socket */
void QHalRemoteComponent::halrcmdMessageReceived(pb::Container *rx)
{
#ifdef QT_DEBUG
    std::string s;
    gpb::TextFormat::PrintToString(*rx, &s);
    DEBUG_TAG(3, m_name, "server message" << QString::fromStdString(s))
#endif

    if (rx->type() == pb::MT_HALRCOMP_BIND_CONFIRM)
    {
#ifdef QT_DEBUG
        DEBUG_TAG(1, m_name,  "bind confirmed")
#endif

        m_subscriber->setReady(true);
    }
    else if ((rx->type() == pb::MT_HALRCOMP_BIND_REJECT)
             || (rx->type() == pb::MT_HALRCOMP_SET_REJECT))
    {
        QString errorString;

        for (int i = 0; i < rx->note_size(); ++i)
        {
            errorString.append(QString::fromStdString(rx->note(i)) + "\n");
        }

        if (rx->type() == pb::MT_HALRCOMP_BIND_REJECT)
        {
            m_rpcClient->setReady(false);
            updateState(Error, BindError, errorString);
        }
        else
        {
            updateState(Error, PinChangeError, errorString);
        }

#ifdef QT_DEBUG
        if (rx->type() == pb::MT_HALRCOMP_BIND_REJECT) {
            DEBUG_TAG(1, m_name, "bind rejected" << errorString)
        }
        else {
            DEBUG_TAG(1, m_name, "pin change rejected" << QString::fromStdString(rx->note(0)))
        }
#endif
    }
    else
    {
#ifdef QT_DEBUG
        DEBUG_TAG(1, m_name, "UNKNOWN server message type")
#endif
    }
}

void QHalRemoteComponent::socketStateChanged(SocketState state)
{
    Q_UNUSED(state)
    SocketState subscriberState = m_subscriber->socketState();
    SocketState clientState = m_rpcClient->socketState();

    if ((subscriberState == SocketUp) && (clientState == SocketUp))
    {
        updateState(Connected);
    }
    else if ((subscriberState == SocketTimeout) || (clientState == SocketTimeout))
    {
        updateState(Timeout);
    }
    else if ((subscriberState == SocketTrying) || (clientState == SocketTrying))
    {
        updateState(Connecting);
    }
    else
    {
        updateState(Disconnected);
    }
}

void QHalRemoteComponent::halrcmdStateChanged(SocketState state)
{
    if (state == SocketUp)
    {
        bind();
    }
    else
    {
        m_subscriber->setReady(false);
    }
}

void QHalRemoteComponent::halrcompStateChanged(SocketState state)
{

}
