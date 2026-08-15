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

    void allRomPathsReturnsEveryIndexedPath() {
        QTemporaryDir dir;
        LibraryDatabase db(dir.path() + "/library.db");
        QVERIFY(db.open());
        db.insertGame("/roms/a.nes", "nes", "A");
        db.insertGame("/roms/b.sfc", "snes", "B");
        QStringList paths = db.allRomPaths();
        QCOMPARE(paths.size(), 2);
        QVERIFY(paths.contains("/roms/a.nes"));
        QVERIFY(paths.contains("/roms/b.sfc"));
    }

    void removeGameDeletesTheMatchingRow() {
        QTemporaryDir dir;
        LibraryDatabase db(dir.path() + "/library.db");
        QVERIFY(db.open());
        db.insertGame("/roms/a.nes", "nes", "A");
        QCOMPARE(db.gameCount(), 1);
        db.removeGame("/roms/a.nes");
        QCOMPARE(db.gameCount(), 0);
    }
};

QTEST_MAIN(LibraryDatabaseTest)
#include "LibraryDatabaseTest.moc"
