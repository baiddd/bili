#include <QtTest>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include "library/RomScanner.h"
#include "storage/LibraryDatabase.h"

class RomScannerTest : public QObject {
    Q_OBJECT
private slots:
    void detectsKnownSystemsByExtension() {
        QCOMPARE(RomScanner::detectSystem("Super Mario World (USA).sfc"), QString("snes"));
        QCOMPARE(RomScanner::detectSystem("Super Mario World (USA).smc"), QString("snes"));
        QCOMPARE(RomScanner::detectSystem("Zelda.nes"), QString("nes"));
        QCOMPARE(RomScanner::detectSystem("Pokemon.gba"), QString("gba"));
        QCOMPARE(RomScanner::detectSystem("Pokemon (Rev 1).gb"), QString("gb"));
        QCOMPARE(RomScanner::detectSystem("Pokemon.gbc"), QString("gb"));
        QCOMPARE(RomScanner::detectSystem("Mario64.n64"), QString("n64"));
        QCOMPARE(RomScanner::detectSystem("Mario64.z64"), QString("n64"));
        QCOMPARE(RomScanner::detectSystem("Sonic.md"), QString("genesis"));
        QCOMPARE(RomScanner::detectSystem("Sonic.gen"), QString("genesis"));
    }

    void detectSystemIsCaseInsensitiveOnExtension() {
        QCOMPARE(RomScanner::detectSystem("Zelda.NES"), QString("nes"));
    }

    void detectSystemReturnsEmptyForUnknownExtension() {
        QCOMPARE(RomScanner::detectSystem("readme.txt"), QString(""));
        QCOMPARE(RomScanner::detectSystem("noextension"), QString(""));
    }

    void cleanTitleStripsTagsAndExtension() {
        QCOMPARE(RomScanner::cleanTitle("Super Mario World (USA).sfc"), QString("Super Mario World"));
        QCOMPARE(RomScanner::cleanTitle("Sonic the Hedgehog (Europe) (Rev 1).md"), QString("Sonic the Hedgehog"));
        QCOMPARE(RomScanner::cleanTitle("Contra [!].nes"), QString("Contra"));
        QCOMPARE(RomScanner::cleanTitle("Kirby's Dream Land (USA, Europe).gb"), QString("Kirby's Dream Land"));
    }

    void cleanTitleHandlesNoTagsGracefully() {
        QCOMPARE(RomScanner::cleanTitle("Tetris.gb"), QString("Tetris"));
    }

    void scanDirectoryIndexesRecognizedFilesAndSkipsOthers() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile(dir.path() + "/Zelda.nes").open(QIODevice::WriteOnly);
        QFile(dir.path() + "/readme.txt").open(QIODevice::WriteOnly);
        QDir(dir.path()).mkdir("sub");
        QFile(dir.path() + "/sub/Mario.sfc").open(QIODevice::WriteOnly);

        LibraryDatabase db(dir.path() + "/library.db");
        QVERIFY(db.open());

        int found = RomScanner::scanDirectory(dir.path(), db);
        QCOMPARE(found, 2);
        QCOMPARE(db.gameCount(), 2);
        QStringList paths = db.allRomPaths();
        QVERIFY(paths.contains(dir.path() + "/Zelda.nes"));
        QVERIFY(paths.contains(dir.path() + "/sub/Mario.sfc"));
    }

    void scanDirectoryRemovesEntriesForDeletedFiles() {
        QTemporaryDir dir;
        QFile romFile(dir.path() + "/Zelda.nes");
        romFile.open(QIODevice::WriteOnly);
        romFile.close();

        LibraryDatabase db(dir.path() + "/library.db");
        QVERIFY(db.open());
        RomScanner::scanDirectory(dir.path(), db);
        QCOMPARE(db.gameCount(), 1);

        QFile::remove(dir.path() + "/Zelda.nes");
        RomScanner::scanDirectory(dir.path(), db);
        QCOMPARE(db.gameCount(), 0);
    }

    void scanDirectoryIsIdempotentOnUnchangedFiles() {
        QTemporaryDir dir;
        QFile(dir.path() + "/Zelda.nes").open(QIODevice::WriteOnly);

        LibraryDatabase db(dir.path() + "/library.db");
        QVERIFY(db.open());
        RomScanner::scanDirectory(dir.path(), db);
        RomScanner::scanDirectory(dir.path(), db);
        QCOMPARE(db.gameCount(), 1);
    }

    void scanDirectoryDoesNotRemoveEntriesFromSiblingDirectoryWithSharedPrefix() {
        QTemporaryDir baseDir;
        QVERIFY(baseDir.isValid());
        QDir(baseDir.path()).mkdir("NES");
        QDir(baseDir.path()).mkdir("NES2");
        QFile(baseDir.path() + "/NES/Zelda.nes").open(QIODevice::WriteOnly);
        QFile(baseDir.path() + "/NES2/Other.nes").open(QIODevice::WriteOnly);

        LibraryDatabase db(baseDir.path() + "/library.db");
        QVERIFY(db.open());

        // Index the sibling "NES2" source first.
        RomScanner::scanDirectory(baseDir.path() + "/NES2", db);
        QCOMPARE(db.gameCount(), 1);

        // Scanning "NES" (a literal string-prefix of "NES2"'s path) must
        // not remove NES2's entries, since NES2 wasn't part of this scan.
        RomScanner::scanDirectory(baseDir.path() + "/NES", db);

        QStringList paths = db.allRomPaths();
        QVERIFY(paths.contains(baseDir.path() + "/NES2/Other.nes"));
    }

    void scanDirectorySkipsMissingDirectoryWithoutWipingIndex() {
        QTemporaryDir dbDir;
        QTemporaryDir romsDir;
        QVERIFY(dbDir.isValid());
        QVERIFY(romsDir.isValid());
        QFile(romsDir.path() + "/Zelda.nes").open(QIODevice::WriteOnly);

        LibraryDatabase db(dbDir.path() + "/library.db");
        QVERIFY(db.open());
        RomScanner::scanDirectory(romsDir.path(), db);
        QCOMPARE(db.gameCount(), 1);

        // Simulate the source becoming unavailable (e.g. an SD card
        // unplugged, or a network share dropping): remove the entire
        // directory, not just the file inside it.
        QVERIFY(QDir(romsDir.path()).removeRecursively());

        int found = RomScanner::scanDirectory(romsDir.path(), db);
        QCOMPARE(found, 0);
        QCOMPARE(db.gameCount(), 1);
    }
};

QTEST_MAIN(RomScannerTest)
#include "RomScannerTest.moc"
