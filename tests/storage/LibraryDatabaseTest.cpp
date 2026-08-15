#include <QtTest>
#include <QTemporaryDir>
#include "storage/LibraryDatabase.h"

class LibraryDatabaseTest : public QObject {
    Q_OBJECT
private slots:
    void createsSchemaAndInsertsGame() {
        QTemporaryDir dir;
        LibraryDatabase db(dir.path() + "/library.db");
        QVERIFY(db.open());
        QCOMPARE(db.gameCount(), 0);

        qint64 id = db.insertGame("/roms/nes/mario.nes", "nes", "Super Mario Bros.");
        QVERIFY(id > 0);
        QCOMPARE(db.gameCount(), 1);
    }
};

QTEST_MAIN(LibraryDatabaseTest)
#include "LibraryDatabaseTest.moc"
