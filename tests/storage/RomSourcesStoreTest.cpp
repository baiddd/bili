#include <QtTest>
#include <QTemporaryDir>
#include <QUrl>
#include "storage/ConfigStore.h"
#include "storage/RomSourcesStore.h"

class RomSourcesStoreTest : public QObject {
    Q_OBJECT
private slots:
    void addPersistsAcrossReload() {
        QTemporaryDir dir;
        ConfigStore config(dir.path());
        RomSourcesStore store(&config);

        QVERIFY(!store.hasAnySource());
        store.addSource(dir.path() + "/ROMs", "Principal");
        QVERIFY(store.hasAnySource());
        QCOMPARE(store.sources().size(), 1);

        ConfigStore reloadedConfig(dir.path());
        reloadedConfig.load();
        RomSourcesStore reloadedStore(&reloadedConfig);
        QCOMPARE(reloadedStore.sources().size(), 1);
    }

    void removeDropsSource() {
        QTemporaryDir dir;
        ConfigStore config(dir.path());
        RomSourcesStore store(&config);
        store.addSource(dir.path() + "/ROMs", "Principal");
        store.removeSource(dir.path() + "/ROMs");
        QVERIFY(!store.hasAnySource());
    }

    void setEnabledUpdatesFlag() {
        QTemporaryDir dir;
        ConfigStore config(dir.path());
        RomSourcesStore store(&config);
        store.addSource(dir.path() + "/ROMs", "Principal");

        QVariantList before = store.sources();
        QCOMPARE(before.size(), 1);
        QCOMPARE(before.at(0).toMap().value("enabled").toBool(), true);

        store.setEnabled(dir.path() + "/ROMs", false);

        QVariantList after = store.sources();
        QCOMPARE(after.size(), 1);
        QCOMPARE(after.at(0).toMap().value("enabled").toBool(), false);
        // Disabling a source does not remove it.
        QVERIFY(store.hasAnySource());
    }

    void toLocalPathConvertsFileUrl() {
        QTemporaryDir dir;
        ConfigStore config(dir.path());
        RomSourcesStore store(&config);

        const QString path = dir.path() + "/ROMs";
        const QUrl url = QUrl::fromLocalFile(path);
        QCOMPARE(store.toLocalPath(url), path);
    }
};

QTEST_MAIN(RomSourcesStoreTest)
#include "RomSourcesStoreTest.moc"
