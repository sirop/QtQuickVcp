#ifndef QAPPLICATIONPLUGINITEM_H
#define QAPPLICATIONPLUGINITEM_H

#include <QObject>
#include <QUrl>

class QApplicationPluginItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
    Q_PROPERTY(PluginType type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(QUrl mainFile READ mainFile WRITE setMainFile NOTIFY mainFileChanged)

public:
    explicit QApplicationPluginItem(QObject *parent = 0);

    enum PluginType {
        Qt5QmlPlugin = 0,
        PythonPlugin = 1
    };

    QString name() const
    {
        return m_name;
    }

    QString description() const
    {
        return m_description;
    }

    PluginType type() const
    {
        return m_type;
    }

    QUrl mainFile() const
    {
        return m_mainFile;
    }

public slots:
    void setName(QString name)
    {
        if (m_name == name)
            return;

        m_name = name;
        emit nameChanged(name);
    }
    void setDescription(QString description)
    {
        if (m_description == description)
            return;

        m_description = description;
        emit descriptionChanged(description);
    }
    void setType(PluginType type)
    {
        if (m_type == type)
            return;

        m_type = type;
        emit typeChanged(type);
    }
    void setMainFile(QUrl mainFile)
    {
        if (m_mainFile == mainFile)
            return;

        m_mainFile = mainFile;
        emit mainFileChanged(mainFile);
    }

private:
    QString m_name;
    QString m_description;
    PluginType m_type;
    QUrl m_mainFile;

signals:
    void nameChanged(QString name);
    void descriptionChanged(QString description);
    void typeChanged(PluginType type);
    void mainFileChanged(QUrl mainFile);
};

#endif // QAPPLICATIONPLUGINITEM_H
