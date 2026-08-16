#pragma once
#include <QString>
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
    static int scanDirectory(const QString &dirPath, LibraryDatabase &db);
};
