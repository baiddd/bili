#include <QtTest>
#include "netplay/StubNetplaySession.h"

class StubNetplaySessionTest : public QObject {
    Q_OBJECT
private slots:
    void reportsNotImplemented() {
        StubNetplaySession session;
        QVERIFY(!session.isImplemented());
        QVERIFY(session.host().contains("non implémenté"));
        QVERIFY(session.join("127.0.0.1").contains("non implémenté"));
    }
};

QTEST_MAIN(StubNetplaySessionTest)
#include "StubNetplaySessionTest.moc"
