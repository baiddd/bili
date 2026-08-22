#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

class ScreenManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentScreen READ currentScreen NOTIFY currentScreenChanged)
    Q_PROPERTY(QString selectedGameRomPath READ selectedGameRomPath WRITE setSelectedGameRomPath NOTIFY selectedGameChanged)
    Q_PROPERTY(QString selectedGameSystem READ selectedGameSystem WRITE setSelectedGameSystem NOTIFY selectedGameChanged)
    Q_PROPERTY(QString selectedGameTitle READ selectedGameTitle WRITE setSelectedGameTitle NOTIFY selectedGameChanged)
public:
    explicit ScreenManager(QObject *parent = nullptr);
    Q_INVOKABLE void push(const QString &screenName);
    Q_INVOKABLE void pop();
    QString currentScreen() const;
    QString selectedGameRomPath() const;
    void setSelectedGameRomPath(const QString &romPath);
    QString selectedGameSystem() const;
    void setSelectedGameSystem(const QString &system);
    QString selectedGameTitle() const;
    void setSelectedGameTitle(const QString &title);

signals:
    void currentScreenChanged();
    void selectedGameChanged();

private:
    QStringList m_stack{"Boot"};
    QString m_selectedGameRomPath;
    QString m_selectedGameSystem;
    QString m_selectedGameTitle;
};
