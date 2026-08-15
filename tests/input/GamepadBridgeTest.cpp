#include <QtTest>
#include "input/InputManager.h"
#include "input/GamepadBridge.h"

class GamepadBridgeTest : public QObject {
    Q_OBJECT
private slots:
    void startsAndStopsWithoutCrashing() {
        InputManager inputManager;
        GamepadBridge bridge(&inputManager);
        bridge.start();
        QTest::qWait(100);
        bridge.stop();
        QVERIFY(true); // reaching here means no crash/deadlock on shutdown
    }
};

QTEST_MAIN(GamepadBridgeTest)
#include "GamepadBridgeTest.moc"
