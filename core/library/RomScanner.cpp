#include "RomScanner.h"
#include <QFileInfo>
#include <QRegularExpression>
#include <QMap>
#include <QDir>
#include <QDirIterator>
#include <QSet>
#include "miniz.h"

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

namespace {
// Scans a single .zip archive's entries for recognized ROMs. Each
// recognized entry (i.e. detectSystem() on its in-archive name yields a
// non-empty system) is inserted into db with rom_path
// "<archive-path>::<entry-name>" (see scanDirectory's doc comment for the
// "::" convention) and recorded into foundOnDisk/foundCount exactly like a
// plain on-disk file, so scanDirectory's existing incremental-sync pass
// (new-entry insert + stale-entry removal) treats archive entries
// uniformly with regular files without needing its own separate logic.
void scanZipArchive(const QString &archivePath, LibraryDatabase &db,
                     QSet<QString> &foundOnDisk, int &foundCount) {
    mz_zip_archive zipArchive;
    mz_zip_zero_struct(&zipArchive);
    if (!mz_zip_reader_init_file(&zipArchive, archivePath.toLocal8Bit().constData(), 0)) {
        return;
    }

    const mz_uint numFiles = mz_zip_reader_get_num_files(&zipArchive);
    for (mz_uint i = 0; i < numFiles; ++i) {
        if (mz_zip_reader_is_file_a_directory(&zipArchive, i)) continue;

        char nameBuf[1024];
        mz_zip_reader_get_filename(&zipArchive, i, nameBuf, sizeof(nameBuf));
        const QString entryName = QString::fromUtf8(nameBuf);

        const QString system = RomScanner::detectSystem(entryName);
        if (system.isEmpty()) continue;

        const QString romPath = archivePath + "::" + entryName;
        foundOnDisk.insert(romPath);
        foundCount++;

        if (!db.allRomPaths().contains(romPath)) {
            db.insertGame(romPath, system, RomScanner::cleanTitle(entryName));
        }
    }

    mz_zip_reader_end(&zipArchive);
}
}

// Recognized ROM files found directly inside .zip archives are indexed as
// their own rows, with rom_path set to "<archive-path>::<entry-name>" (a
// "::" separator between the archive's own path and the entry's name
// inside it). This is a virtual-indexing convention only: the archive
// itself is never extracted or modified on disk here. Later code that
// needs to actually launch/extract such an entry must split rom_path on
// "::" to recover the archive path and the entry name within it.
//
// .7z archives are not scanned (documented gap - see docs/index.md):
// unlike .zip, .7z is LZMA-based and no lightweight, vendorable
// Windows/MinGW-compatible C/C++ library was found for it without adding
// a much heavier dependency (a full LZMA SDK or a wrapper around 7-Zip's
// own DLLs) - out of scope for this task.
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

        if (QFileInfo(filePath).suffix().compare("zip", Qt::CaseInsensitive) == 0) {
            scanZipArchive(filePath, db, foundOnDisk, foundCount);
            continue;
        }

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
