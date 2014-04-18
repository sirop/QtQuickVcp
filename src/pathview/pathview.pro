TEMPLATE = lib
TARGET = machinekitpathviewplugin
TARGETPATH = Machinekit/PathView
QT += qml quick
CONFIG += qt plugin

TARGET = $$qtLibraryTarget($$TARGET)
uri = Machinekit.PathView

# Input
SOURCES += \
    pathview3d.cpp \
    plugin.cpp

HEADERS += \
    pathview3d.h \
    plugin.h

QML_INFRA_FILES += \
    qmldir
#    src/plugins.qmltypes

include(../deployment.pri)
