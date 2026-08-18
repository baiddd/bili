#include "LibraryScanner.h"
#include "RomScanner.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>

LibraryScanner::LibraryScanner(LibraryDatabase *db, QObject *parent)
    : QObject(parent), m_db(db) {}

void LibraryScanner::startScan(const QVariantList &sources) {
    // Re-entrancy guard: GameList's Connections block (ui/Main.qml) fires
    // startScan() every time GameList is (re-)entered, and the Settings
    // "Rescanner" button can fire it again on demand -- either of which can
    // land while a previous scan is still running its QtConcurrent::run
    // worker. That worker opens its own LibraryDatabase (its own SQLite
    // connection) to the same file passed to this LibraryScanner; a second
    // concurrent worker doing the same thing races it on writes to that
    // same file. Rather than let two workers stomp on each other, treat a
    // call that arrives while a scan is already in flight as a no-op.
    if (m_scanning) return;

    m_cancelRequested = false;
    emit scanStarted();

    // m_db was opened on whatever thread constructed this LibraryScanner
    // (the GUI thread, per Task 6's wiring). Qt's QSqlDatabase docs are
    // explicit that "a QSqlDatabase instance must only be accessed by the
    // thread it was created in" - so the QtConcurrent::run lambda below,
    // which executes on a worker thread from Qt's global thread pool,
    // must NOT touch *m_db* directly. Instead it opens its own
    // LibraryDatabase connection to the same underlying SQLite file
    // (m_db->path()) from within the worker thread itself, so the
    // connection genuinely belongs to that thread. Both connections point
    // at the same file, so writes made through the worker's connection
    // are visible via m_db once committed/flushed.
    const QString dbPath = m_db->path();

    auto *watcher = new QFutureWatcher<int>(this);
    connect(watcher, &QFutureWatcher<int>::finished, this, [this, watcher]() {
        m_scanning = false;
        emit scanFinished(watcher->result());
        watcher->deleteLater();
    });

    m_scanning = true;
    m_future = QtConcurrent::run([this, sources, dbPath]() -> int {
        LibraryDatabase workerDb(dbPath);
        if (!workerDb.open()) return 0;

        int total = 0;
        for (const QVariant &sourceVariant : sources) {
            // Checked between sources too, not just inside
            // RomScanner::scanDirectory's own loop: without this, cancelling
            // while source N is finishing would still march through every
            // remaining not-yet-started source before the worker actually
            // returns, defeating the point of a prompt cancel-and-wait.
            if (m_cancelRequested.load()) break;

            const QVariantMap source = sourceVariant.toMap();
            if (!source.value("enabled").toBool()) continue;
            const QString path = source.value("path").toString();
            const int found = RomScanner::scanDirectory(path, workerDb, &m_cancelRequested);
            total += found;
            emit sourceScanned(path, found);
        }
        return total;
    });
    watcher->setFuture(m_future);
}

void LibraryScanner::cancelAndWait() {
    m_cancelRequested = true;
    if (m_scanning) {
        m_future.waitForFinished();
    }
}
