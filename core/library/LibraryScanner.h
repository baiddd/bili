#pragma once
#include <QObject>
#include <QVariantList>
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

signals:
    void scanStarted();
    void sourceScanned(const QString &path, int filesFound);
    void scanFinished(int totalFilesFound);

private:
    LibraryDatabase *m_db;
};
