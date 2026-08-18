#pragma once
#include <QString>
#include <atomic>
#include "storage/LibraryDatabase.h"

class RomScanner {
public:
    static QString detectSystem(const QString &fileName);
    static QString cleanTitle(const QString &fileName);

    // Recursively scans dirPath for recognized ROM files, including ones
    // found inside .zip archives (.7z is not scanned - see docs/index.md).
    // Archive-internal entries are indexed with rom_path
    // "<archive-path>::<entry-name>" - see the definition in
    // RomScanner.cpp for the full "::" convention.
    //
    // cancelRequested, if non-null, is polled once per iteration of the
    // main file-walk loop; as soon as it reads true the scan stops early
    // (any files not yet visited are simply left for the next scan) rather
    // than grinding through the rest of a large directory. This lets a
    // caller (LibraryScanner::cancelAndWait()) bound how long a scan keeps
    // running after the app has decided to shut down.
    static int scanDirectory(const QString &dirPath, LibraryDatabase &db,
                              const std::atomic<bool> *cancelRequested = nullptr);
};
