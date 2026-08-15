#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "storage/ConfigStore.h"

class ConfigStoreTest : public QObject {
    Q_OBJECT
private slots:
    void savesAndReloadsJson() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        ConfigStore store(dir.path());
        QJsonObject obj;
        obj["theme"] = "dark";
        store.setData(obj);
        QVERIFY(store.save());

        ConfigStore reloaded(dir.path());
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.data()["theme"].toString(), QString("dark"));
    }

    void loadOnMissingFileReturnsFalseWithoutCrashing() {
        QTemporaryDir dir;
        ConfigStore store(dir.path());
        QVERIFY(!store.load());
        QVERIFY(store.data().isEmpty());
    }

    void loadOnMalformedJsonReturnsFalseAndLeavesDataUnchanged() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QFile file(dir.path() + "/config.json");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{ this is not valid json ");
        file.close();

        ConfigStore store(dir.path());
        QVERIFY(!store.load());
        QVERIFY(store.data().isEmpty());
    }
};

QTEST_MAIN(ConfigStoreTest)
#include "ConfigStoreTest.moc"
