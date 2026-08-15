#include <QtTest>
#include "ui/ScreenManager.h"

class ScreenManagerTest : public QObject {
    Q_OBJECT
private slots:
    void startsOnBootAndNavigates() {
        ScreenManager mgr;
        QCOMPARE(mgr.currentScreen(), QString("Boot"));

        QSignalSpy spy(&mgr, &ScreenManager::currentScreenChanged);
        mgr.push("MainMenu");
        QCOMPARE(mgr.currentScreen(), QString("MainMenu"));
        QCOMPARE(spy.count(), 1);
    }

    void popReturnsToPreviousScreen() {
        ScreenManager mgr;
        mgr.push("MainMenu");
        mgr.push("Settings");
        mgr.pop();
        QCOMPARE(mgr.currentScreen(), QString("MainMenu"));
    }
};

QTEST_MAIN(ScreenManagerTest)
#include "ScreenManagerTest.moc"
