#pragma once
#include <QString>

class RomScanner {
public:
    static QString detectSystem(const QString &fileName);
    static QString cleanTitle(const QString &fileName);
};
