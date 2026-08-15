#include <QtTest>
#include "emulators/StubEmulatorProvider.h"

class StubEmulatorProviderTest : public QObject {
    Q_OBJECT
private slots:
    void reportsNotImplemented() {
        StubEmulatorProvider provider;
        QVERIFY(!provider.isImplemented());
        QVERIFY(provider.installRetroArch().contains("non implémenté"));
        QVERIFY(provider.installCore("nes").contains("non implémenté"));
        QVERIFY(provider.installStandaloneEmulator("Dolphin").contains("non implémenté"));
    }
};

QTEST_MAIN(StubEmulatorProviderTest)
#include "StubEmulatorProviderTest.moc"
