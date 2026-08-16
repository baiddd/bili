#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include "library/LibraryScanner.h"
#include "storage/LibraryDatabase.h"

class LibraryScannerTest : public QObject {
    Q_OBJECT
private slots:
    void scansAllEnabledSourcesAndEmitsFinished() {
        QTemporaryDir dir1, dir2;
        QFile(dir1.path() + "/Zelda.nes").open(QIODevice::WriteOnly);
        QFile(dir2.path() + "/Mario.sfc").open(QIODevice::WriteOnly);

        QTemporaryDir dbDir;
        LibraryDatabase db(dbDir.path() + "/library.db");
        QVERIFY(db.open());

        LibraryScanner scanner(&db);
        QSignalSpy startedSpy(&scanner, &LibraryScanner::scanStarted);
        QSignalSpy sourceSpy(&scanner, &LibraryScanner::sourceScanned);
        QSignalSpy finishedSpy(&scanner, &LibraryScanner::scanFinished);

        QVariantList sources;
        QVariantMap s1; s1["path"] = dir1.path(); s1["label"] = "A"; s1["enabled"] = true;
        QVariantMap s2; s2["path"] = dir2.path(); s2["label"] = "B"; s2["enabled"] = false;
        sources << s1 << s2;

        scanner.startScan(sources);
        QVERIFY(finishedSpy.wait(5000));

        QCOMPARE(startedSpy.count(), 1);
        QCOMPARE(sourceSpy.count(), 1); // only the enabled source
        QCOMPARE(finishedSpy.at(0).at(0).toInt(), 1); // 1 file found total
        QCOMPARE(db.gameCount(), 1);
    }

    void secondStartScanWhileFirstInFlightIsNoOp() {
        QTemporaryDir dir1;
        QFile(dir1.path() + "/Zelda.nes").open(QIODevice::WriteOnly);

        QTemporaryDir dbDir;
        LibraryDatabase db(dbDir.path() + "/library.db");
        QVERIFY(db.open());

        LibraryScanner scanner(&db);
        QSignalSpy startedSpy(&scanner, &LibraryScanner::scanStarted);
        QSignalSpy finishedSpy(&scanner, &LibraryScanner::scanFinished);

        QVariantList sources;
        QVariantMap s1; s1["path"] = dir1.path(); s1["label"] = "A"; s1["enabled"] = true;
        sources << s1;

        // Call startScan() twice back-to-back, with no event-loop spin in
        // between, simulating rapid re-entry into GameList (or a Rescanner
        // click while a boot-triggered scan is still in flight). Because
        // m_scanning is set synchronously before QtConcurrent::run is
        // dispatched, the second call must see the guard already up and
        // become a no-op -- regardless of how fast the worker thread runs --
        // so this assertion is deterministic, not timing-dependent.
        scanner.startScan(sources);
        scanner.startScan(sources);

        QCOMPARE(startedSpy.count(), 1);

        QVERIFY(finishedSpy.wait(5000));
        QCOMPARE(finishedSpy.count(), 1);
    }
};

QTEST_MAIN(LibraryScannerTest)
#include "LibraryScannerTest.moc"
