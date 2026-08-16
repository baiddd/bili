#include "RomScanner.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QMap>
#include <QDir>
#include <QDirIterator>
#include <QSet>

QString RomScanner::detectSystem(const QString &fileName) {
    static const QMap<QString, QString> kExtensionToSystem = {
        {"nes", "nes"},
        {"sfc", "snes"}, {"smc", "snes"},
        {"gba", "gba"},
        {"gb", "gb"}, {"gbc", "gb"},
        {"n64", "n64"}, {"z64", "n64"},
        {"md", "genesis"}, {"gen", "genesis"},
    };
    const QString ext = QFileInfo(fileName).suffix().toLower();
    return kExtensionToSystem.value(ext, QString());
}

QString RomScanner::cleanTitle(const QString &fileName) {
    QString base = QFileInfo(fileName).completeBaseName();
    static const QRegularExpression kTagPattern(R"(\s*[\(\[][^\)\]]*[\)\]]\s*)");
    base.replace(kTagPattern, " ");
    return base.trimmed();
}

int RomScanner::scanDirectory(const QString &dirPath, LibraryDatabase &db) {
    // A source directory that's temporarily unreachable (unplugged SD
    // card, unmounted network share) must never wipe its previously
    // indexed entries — skip the scan entirely rather than treating
    // "found nothing" as "everything under here was deleted".
    if (!QDir(dirPath).exists()) {
        return 0;
    }

    // Normalize once so every comparison below (and QDirIterator's own
    // traversal) agrees on the same form, regardless of whether the
    // caller passed a trailing slash (e.g. "D:/ROMs/NES/" from a
    // directory picker or user-typed config). Without this, the
    // path-boundary check below would compare against a double slash
    // ("D:/ROMs/NES//") that no real entry ever matches, silently
    // disabling stale-entry removal for that source.
    const QString normalizedDir = QDir::cleanPath(dirPath);

    QSet<QString> foundOnDisk;
    int foundCount = 0;

    QDirIterator it(normalizedDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filePath = it.next();
        const QString system = detectSystem(filePath);
        if (system.isEmpty()) continue;

        foundOnDisk.insert(filePath);
        foundCount++;

        if (!db.allRomPaths().contains(filePath)) {
            db.insertGame(filePath, system, cleanTitle(filePath));
        }
    }

    for (const QString &existingPath : db.allRomPaths()) {
        // Path-boundary-aware prefix check: a raw startsWith(dirPath)
        // would also match a sibling source directory whose name happens
        // to extend dirPath's string (e.g. "D:/ROMs/NES" matching
        // "D:/ROMs/NES2/mario.nes"), silently deleting entries that
        // belong to a different, unscanned source.
        const bool underDir = existingPath == normalizedDir || existingPath.startsWith(normalizedDir + "/");
        if (underDir && !foundOnDisk.contains(existingPath)) {
            db.removeGame(existingPath);
        }
    }

    return foundCount;
}
