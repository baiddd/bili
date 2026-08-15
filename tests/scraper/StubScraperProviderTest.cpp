#include <QtTest>
#include "scraper/StubScraperProvider.h"

class StubScraperProviderTest : public QObject {
    Q_OBJECT
private slots:
    void reportsNotImplemented() {
        StubScraperProvider provider;
        QVERIFY(!provider.isImplemented());
        QVERIFY(provider.scrapeGame(1).contains("non implémenté"));
        QVERIFY(provider.scrapeLibrary().contains("non implémenté"));
    }
};

QTEST_MAIN(StubScraperProviderTest)
#include "StubScraperProviderTest.moc"
