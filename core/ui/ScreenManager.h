#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

class ScreenManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentScreen READ currentScreen NOTIFY currentScreenChanged)
public:
    explicit ScreenManager(QObject *parent = nullptr);
    Q_INVOKABLE void push(const QString &screenName);
    Q_INVOKABLE void pop();
    QString currentScreen() const;

signals:
    void currentScreenChanged();

private:
    QStringList m_stack{"Boot"};
};
