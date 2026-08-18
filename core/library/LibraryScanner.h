#pragma once
#include <QObject>
#include <QVariantList>
#include <QFuture>
#include <atomic>
#include "storage/LibraryDatabase.h"

// Runs a full library scan (every enabled RomSource) on Qt's global thread
// pool via QtConcurrent::run, so the caller's thread (the GUI thread, in
// practice) never blocks. See LibraryScanner.cpp for why the worker opens
// its own LibraryDatabase connection instead of reusing the one passed to
// the constructor: QSqlDatabase connections are not safe to use from a
// thread other than the one that created them.
class LibraryScanner : public QObject {
    Q_OBJECT
public:
    explicit LibraryScanner(LibraryDatabase *db, QObject *parent = nullptr);

    // Scans every enabled RomSource in sequence, on a background thread.
    // sources: the QVariantList from RomSourcesStore::sources().
    Q_INVOKABLE void startScan(const QVariantList &sources);

    // Requests that any in-flight scan stop as soon as practical, then
    // blocks until the worker thread has actually returned. Must be called
    // (and allowed to complete) before this LibraryScanner or the
    // LibraryDatabase it wraps get destroyed - otherwise Qt's global
    // QThreadPool static destructor blocks process exit until the worker
    // finishes on its own (which, for a large/slow source, can take
    // minutes), and/or the worker's lambda can touch a LibraryScanner that
    // no longer exists. Safe to call even when no scan is running.
    void cancelAndWait();

signals:
    void scanStarted();
    void sourceScanned(const QString &path, int filesFound);
    void scanFinished(int totalFilesFound);

private:
    LibraryDatabase *m_db;
    bool m_scanning = false;
    std::atomic<bool> m_cancelRequested{false};
    QFuture<int> m_future;
};
