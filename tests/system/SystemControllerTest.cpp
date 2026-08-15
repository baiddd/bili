#include <QtTest>
#include "system/SystemController.h"

class SystemControllerTest : public QObject {
    Q_OBJECT
private slots:
    void restartUsesWindowsShutdownCommandWithRestartFlag() {
        QCOMPARE(SystemController::programName(), QString("shutdown"));
        QCOMPARE(SystemController::restartArgs(), QStringList({"/r", "/t", "0"}));
    }

    void shutdownUsesWindowsShutdownCommandWithPowerOffFlag() {
        QCOMPARE(SystemController::programName(), QString("shutdown"));
        QCOMPARE(SystemController::shutdownArgs(), QStringList({"/s", "/t", "0"}));
    }

    // Deliberately NOT calling restartSystem()/shutdownSystem() anywhere in
    // this suite -- they invoke a REAL OS restart/shutdown via
    // QProcess::startDetached. Only the command-construction helpers above
    // are exercised. quitApplication() is verified manually instead
    // (Step 6), not unit-tested here.
};

QTEST_MAIN(SystemControllerTest)
#include "SystemControllerTest.moc"
