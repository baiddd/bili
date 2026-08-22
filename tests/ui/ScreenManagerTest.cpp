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

    void selectedGamePropertiesRoundTripAndEmitOneSignal() {
        ScreenManager mgr;
        QSignalSpy spy(&mgr, &ScreenManager::selectedGameChanged);

        mgr.setSelectedGameRomPath("C:/roms/Zelda.nes");
        mgr.setSelectedGameSystem("nes");
        mgr.setSelectedGameTitle("Zelda");

        QCOMPARE(mgr.selectedGameRomPath(), QString("C:/roms/Zelda.nes"));
        QCOMPARE(mgr.selectedGameSystem(), QString("nes"));
        QCOMPARE(mgr.selectedGameTitle(), QString("Zelda"));
        QCOMPARE(spy.count(), 3);

        // Setting the same value again must not re-emit (standard Qt
        // property-setter hygiene: avoids redundant QML re-bindings).
        mgr.setSelectedGameTitle("Zelda");
        QCOMPARE(spy.count(), 3);
    }
};

QTEST_MAIN(ScreenManagerTest)
#include "ScreenManagerTest.moc"
