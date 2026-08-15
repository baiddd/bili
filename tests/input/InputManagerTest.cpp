#include <QtTest>
#include <Qt>
#include "input/InputManager.h"

class InputManagerTest : public QObject {
    Q_OBJECT
private slots:
    void arrowUpEmitsNavigateUp() {
        InputManager mgr;
        QSignalSpy spy(&mgr, &InputManager::navigateUp);
        mgr.handleKeyPress(Qt::Key_Up);
        QCOMPARE(spy.count(), 1);
    }

    void enterEmitsAccept() {
        InputManager mgr;
        QSignalSpy spy(&mgr, &InputManager::accept);
        mgr.handleKeyPress(Qt::Key_Return);
        QCOMPARE(spy.count(), 1);
    }

    void escapeEmitsCancel() {
        InputManager mgr;
        QSignalSpy spy(&mgr, &InputManager::cancel);
        mgr.handleKeyPress(Qt::Key_Escape);
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(InputManagerTest)
#include "InputManagerTest.moc"
