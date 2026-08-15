#include "SystemController.h"
#include <QProcess>
#include <QCoreApplication>

SystemController::SystemController(QObject *parent) : QObject(parent) {}

QString SystemController::programName() { return "shutdown"; }
QStringList SystemController::restartArgs() { return {"/r", "/t", "0"}; }
QStringList SystemController::shutdownArgs() { return {"/s", "/t", "0"}; }

void SystemController::restartSystem() {
    QProcess::startDetached(programName(), restartArgs());
}

void SystemController::shutdownSystem() {
    QProcess::startDetached(programName(), shutdownArgs());
}

void SystemController::quitApplication() {
    QCoreApplication::quit();
}
