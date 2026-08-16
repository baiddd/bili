#include "LibraryScanner.h"
#include "RomScanner.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>

LibraryScanner::LibraryScanner(LibraryDatabase *db, QObject *parent)
    : QObject(parent), m_db(db) {}

void LibraryScanner::startScan(const QVariantList &sources) {
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
        emit scanFinished(watcher->result());
        watcher->deleteLater();
    });

    QFuture<int> future = QtConcurrent::run([this, sources, dbPath]() -> int {
        LibraryDatabase workerDb(dbPath);
        if (!workerDb.open()) return 0;

        int total = 0;
        for (const QVariant &sourceVariant : sources) {
            const QVariantMap source = sourceVariant.toMap();
            if (!source.value("enabled").toBool()) continue;
            const QString path = source.value("path").toString();
            const int found = RomScanner::scanDirectory(path, workerDb);
            total += found;
            emit sourceScanned(path, found);
        }
        return total;
    });
    watcher->setFuture(future);
}
