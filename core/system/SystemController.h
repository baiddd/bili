#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

class SystemController : public QObject {
    Q_OBJECT
public:
    explicit SystemController(QObject *parent = nullptr);
    Q_INVOKABLE void restartSystem();
    Q_INVOKABLE void shutdownSystem();
    Q_INVOKABLE void quitApplication();

    // Exposed for testing only: the exact program/args restart/shutdown
    // would invoke via QProcess::startDetached, without ever running them.
    static QString programName();
    static QStringList restartArgs();
    static QStringList shutdownArgs();
};
