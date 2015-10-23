#ifndef QAPPLICATIONPLUGINS_H
#define QAPPLICATIONPLUGINS_H

#include <QQuickItem>
#include <QQmlListProperty>
#include <QDir>
#include <QDirIterator>
#include <QSettings>
#include "qapplicationpluginitem.h"

class QApplicationPlugins : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<QApplicationPluginItem> plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(QStringList searchPaths READ searchPaths WRITE setSearchPaths NOTIFY searchPathsChanged)

public:
    QApplicationPlugins(QQuickItem *parent = 0);

    QQmlListProperty<QApplicationPluginItem> plugins();
    int pluginCount() const;
    QApplicationPluginItem *plugin(int index) const;

    QStringList searchPaths() const
    {
        return m_searchPaths;
    }

public slots:
    void updatePlugins();
    void clearPlugins();

    void setSearchPaths(QStringList searchPaths)
    {
        if (m_searchPaths == searchPaths)
            return;

        m_searchPaths = searchPaths;
        emit searchPathsChanged(searchPaths);
    }

private:
    QList<QApplicationPluginItem*> m_plugins;
    QStringList m_searchPaths;

    void readPluginFile(QString filePath);

signals:
    void searchPathsChanged(QStringList searchPaths);
    void pluginsChanged(QQmlListProperty<QApplicationPluginItem> arg);
};

#endif // QAPPLICATIONPLUGINS_H
