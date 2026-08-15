#pragma once
#include <QString>
#include "storage/LibraryDatabase.h"

class RomScanner {
public:
    static QString detectSystem(const QString &fileName);
    static QString cleanTitle(const QString &fileName);
    static int scanDirectory(const QString &dirPath, LibraryDatabase &db);
};
