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
    QSet<QString> foundOnDisk;
    int foundCount = 0;

    QDirIterator it(dirPath, QDir::Files, QDirIterator::Subdirectories);
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
        if (existingPath.startsWith(dirPath) && !foundOnDisk.contains(existingPath)) {
            db.removeGame(existingPath);
        }
    }

    return foundCount;
}
