#include "EmulatorProvider.h"
#include "miniz.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QProcess>
#include <QTextStream>

QString EmulatorProvider::s_sevenZipPathOverride;

EmulatorProvider::EmulatorProvider(QString dataDir, NetworkManager *networkManager, QObject *parent)
    : QObject(parent), m_dataDir(std::move(dataDir)), m_networkManager(networkManager) {
    connect(m_networkManager, &NetworkManager::progress, this,
            [this](int requestId, qint64 received, qint64 total) {
        const QString target = m_activeDownloadTargets.value(requestId);
        if (target.isEmpty()) return;
        emit installProgress(target, received, total);
    });

    connect(m_networkManager, &NetworkManager::finished, this,
            [this](int requestId, const QString &destPath) {
        if (!m_activeDownloadTargets.contains(requestId)) return;
        const QString target = m_activeDownloadTargets.take(requestId);

        if (target.startsWith("core:")) {
            const QString system = target.mid(QString("core:").size());
            const QString coreName = m_pendingCoreFilenames.take(target);
            QDir().mkpath(coresDir());
            if (!extractZipEntry(destPath, coreName + "_libretro.dll", coresDir())) {
                QFile::remove(destPath);
                emit installFailed(target, "Échec de l'extraction du core.");
                return;
            }
            QFile::remove(destPath);
            InstalledState state = readInstalledState();
            state.coresBySystem.insert(system, coreName);
            persistInstalledState(state);
            emit installFinished(target);
            return;
        }

        if (target == "retroarch") {
            if (!extract7zArchive(destPath, retroArchDir())) {
                QFile::remove(destPath);
                emit installFailed(target, "Échec de l'extraction de RetroArch.");
                return;
            }
            QFile::remove(destPath);
            writePortableRetroArchConfig();
            InstalledState state = readInstalledState();
            state.retroArch = true;
            persistInstalledState(state);
            emit installFinished(target);
            return;
        }
    });

    connect(m_networkManager, &NetworkManager::failed, this,
            [this](int requestId, const QString &errorString) {
        const QString target = m_activeDownloadTargets.take(requestId);
        if (target.isEmpty()) return;
        m_pendingCoreFilenames.remove(target);
        emit installFailed(target, errorString);
    });
}

EmulatorProvider::~EmulatorProvider() {
    delete m_gameTempDir; // removes the extracted-entry temp dir, if any
}

QString EmulatorProvider::retroArchDir() const {
    return m_dataDir + "/emulators/retroarch";
}

QString EmulatorProvider::retroArchExecutablePath() const {
    return retroArchDir() + "/retroarch.exe";
}

QString EmulatorProvider::coresDir() const {
    return retroArchDir() + "/cores";
}

QString EmulatorProvider::installedStatePath() const {
    return m_dataDir + "/emulators/installed.json";
}

EmulatorProvider::InstalledState EmulatorProvider::readInstalledState() const {
    InstalledState state;
    QFile file(installedStatePath());
    if (!file.open(QIODevice::ReadOnly)) return state;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return state;

    const QJsonObject obj = doc.object();
    state.retroArch = obj.value("retroarch").toBool(false);
    const QJsonObject cores = obj.value("cores").toObject();
    for (auto it = cores.begin(); it != cores.end(); ++it) {
        state.coresBySystem.insert(it.key(), it.value().toString());
    }
    return state;
}

bool EmulatorProvider::isRetroArchInstalled() const {
    const InstalledState state = readInstalledState();
    return state.retroArch && QFile::exists(retroArchExecutablePath());
}

bool EmulatorProvider::isCoreInstalled(const QString &system) const {
    const InstalledState state = readInstalledState();
    const QString core = state.coresBySystem.value(system);
    if (core.isEmpty()) return false;
    return QFile::exists(coresDir() + "/" + core + "_libretro.dll");
}

void EmulatorProvider::installCore(const QString &system) {
    const CoreCatalogEntry entry = m_catalogData.coresBySystem.value(system);
    if (entry.core.isEmpty()) {
        emit installFailed("core:" + system, "Catalogue non chargé — réessaie.");
        return;
    }
    installCoreFrom(system, entry);
}

void EmulatorProvider::installCoreFrom(const QString &system, const CoreCatalogEntry &entry) {
    const QString target = "core:" + system;
    QTemporaryFile temp;
    temp.setAutoRemove(false);
    temp.open();
    const QString tempPath = temp.fileName();
    temp.close();

    const int requestId = m_networkManager->startDownload(entry.url, tempPath);
    m_activeDownloadTargets.insert(requestId, target);
    m_pendingCoreFilenames.insert(target, entry.core);
}

void EmulatorProvider::installRetroArch() {
    if (m_catalogData.retroArchUrl.isEmpty()) {
        emit installFailed("retroarch", "Catalogue non chargé — réessaie.");
        return;
    }
    installRetroArchFrom(m_catalogData.retroArchUrl);
}

void EmulatorProvider::installRetroArchFrom(const QUrl &url) {
    QTemporaryFile temp;
    temp.setAutoRemove(false);
    temp.open();
    const QString tempPath = temp.fileName();
    temp.close();

    const int requestId = m_networkManager->startDownload(url, tempPath);
    m_activeDownloadTargets.insert(requestId, "retroarch");
}

void EmulatorProvider::uninstallCore(const QString &system) {
    const InstalledState state = readInstalledState();
    const QString core = state.coresBySystem.value(system);
    const QString target = "core:" + system;
    if (core.isEmpty()) {
        emit uninstallFailed(target, "Ce core n'est pas installé.");
        return;
    }

    const QString path = coresDir() + "/" + core + "_libretro.dll";
    if (QFile::exists(path) && !QFile::remove(path)) {
        emit uninstallFailed(target, "Impossible de supprimer le fichier du core.");
        return;
    }

    InstalledState newState = state;
    newState.coresBySystem.remove(system);
    persistInstalledState(newState);

    emit uninstallFinished(target);
}

void EmulatorProvider::uninstallRetroArch() {
    if (!QDir(retroArchDir()).removeRecursively()) {
        // removeRecursively() also returns true if the directory simply
        // doesn't exist, so a false result here is a genuine failure
        // (e.g. a file still open/locked), not "already uninstalled".
        emit uninstallFailed("retroarch", "Impossible de supprimer RetroArch (fichier verrouillé ?).");
        return;
    }

    InstalledState state = readInstalledState();
    state.retroArch = false;
    persistInstalledState(state);

    emit uninstallFinished("retroarch");
}

QStringList EmulatorProvider::launchArgs(const QString &corePath, const QString &resolvedRomPath) {
    return {"-L", corePath, resolvedRomPath};
}

void EmulatorProvider::launchGame(const QString &romPath, const QString &system) {
    const InstalledState state = readInstalledState();
    const QString core = state.coresBySystem.value(system);
    if (core.isEmpty() || !isCoreInstalled(system)) {
        emit launchFailed("Aucun core installé pour ce système.");
        return;
    }
    if (!isRetroArchInstalled()) {
        emit launchFailed("RetroArch n'est pas installé.");
        return;
    }

    // Research (Task 6): neither RetroArch's own CLI guide
    // (docs.libretro.com/guides/cli-intro) nor its retroarch(6) man page
    // document any command-line syntax for pointing directly at a file
    // inside a .zip archive. RetroArch's UI can browse into an archive and
    // internally builds a path of the form "archive.zip#entry.rom" when a
    // user picks content that way, but multiple open RetroArch issues
    // (github.com/libretro/RetroArch/issues/12577, #15416, #15424) show that
    // exact "#"-separated form failing to load when supplied on the command
    // line instead -- it's an internal convention, not a documented/
    // guaranteed-stable public argument format. Per the plan's guidance to
    // fall back to extraction rather than ship an unverified syntax: split
    // Bili's own "<archive>::<entry>" rom_path convention (sub-project 2's
    // RomScanner), extract that entry to a temp file, and point RetroArch at
    // the extracted file instead.
    QString resolvedRomPath = romPath;
    delete m_gameTempDir;
    m_gameTempDir = nullptr;
    const int separatorIndex = romPath.indexOf("::");
    if (separatorIndex != -1) {
        const QString archivePath = romPath.left(separatorIndex);
        const QString entryName = romPath.mid(separatorIndex + 2);

        m_gameTempDir = new QTemporaryDir();
        if (!m_gameTempDir->isValid() || !extractZipEntry(archivePath, entryName, m_gameTempDir->path())) {
            delete m_gameTempDir;
            m_gameTempDir = nullptr;
            emit launchFailed("Impossible d'extraire le jeu de l'archive.");
            return;
        }
        resolvedRomPath = m_gameTempDir->path() + "/" + QFileInfo(entryName).fileName();
    }

    const QString corePath = coresDir() + "/" + core + "_libretro.dll";

    if (m_gameProcess) {
        m_gameProcess->deleteLater();
    }
    m_gameProcess = new QProcess(this);
    connect(m_gameProcess, &QProcess::started, this, [this]() { emit gameLaunched(); });
    connect(m_gameProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus) { emit gameExited(exitCode); });
    connect(m_gameProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError) { emit launchFailed("Impossible de lancer RetroArch."); });

    m_gameProcess->start(retroArchExecutablePath(), launchArgs(corePath, resolvedRomPath));
}

QString EmulatorProvider::sevenZipExecutablePath() {
    if (!s_sevenZipPathOverride.isEmpty()) return s_sevenZipPathOverride;
    return QStandardPaths::findExecutable("7za", {QCoreApplication::applicationDirPath()});
}

void EmulatorProvider::setSevenZipExecutablePathForTesting(const QString &path) {
    s_sevenZipPathOverride = path;
}

bool EmulatorProvider::extract7zArchive(const QString &archivePath, const QString &destDir) {
    const QString sevenZip = sevenZipExecutablePath();
    if (sevenZip.isEmpty()) return false;

    QDir().mkpath(destDir);
    QProcess process;
    process.start(sevenZip, {"x", archivePath, "-o" + destDir, "-y"});
    if (!process.waitForFinished(60000) || process.exitCode() != 0) {
        QDir(destDir).removeRecursively();
        return false;
    }

    // The official RetroArch.7z (verified against a real download from
    // buildbot.libretro.com/stable/1.22.2/windows/x86_64/RetroArch.7z) wraps
    // its entire contents in a single top-level folder ("RetroArch-Win64/")
    // rather than placing retroarch.exe at the archive root, and 7-Zip's "x"
    // command has no "strip leading path component" switch the way tar
    // does. Promote that folder's contents up into destDir so
    // retroArchExecutablePath() (destDir + "/retroarch.exe") finds the exe
    // directly. A no-op for the test's fake 7za.exe stand-in, which writes
    // retroarch.exe straight into destDir with no nesting.
    if (!QFile::exists(destDir + "/retroarch.exe")) {
        const QStringList topLevelDirs = QDir(destDir).entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        if (topLevelDirs.size() == 1) {
            const QString nested = destDir + "/" + topLevelDirs.first();
            if (QFile::exists(nested + "/retroarch.exe")) {
                const QStringList items = QDir(nested).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
                for (const QString &item : items) {
                    QDir().rename(nested + "/" + item, destDir + "/" + item);
                }
                QDir().rmdir(nested);
            }
        }
    }

    // Only report success once retroarch.exe genuinely landed at the
    // expected path. Without this check, either (a) extraction producing
    // more than one top-level subdirectory (the flatten guard above only
    // handles exactly one) or (b) a single subdirectory that itself doesn't
    // contain retroarch.exe (e.g. a partial/corrupt extraction where
    // 7za.exe still exits 0) would silently be treated as a successful
    // install by the caller, which persists installed.json and deletes the
    // downloaded archive -- destroying the only way to retry without
    // re-downloading. Clean up any partial extraction on failure so a later
    // retry doesn't find debris left behind here.
    if (!QFile::exists(destDir + "/retroarch.exe")) {
        QDir(destDir).removeRecursively();
        return false;
    }

    return true;
}

void EmulatorProvider::writePortableRetroArchConfig() const {
    // Research (Task 4): a real downloaded RetroArch.7z from
    // buildbot.libretro.com, plus RetroArch's own source
    // (frontend/drivers/platform_win32.c and libretro-common/file/
    // file_path.c's fill_pathname_expand_special), shows the plain Windows
    // .7z build is already portable by default — every directory setting
    // defaults to a ":"-prefixed path (":\\system", ":\\saves", ":\\states")
    // that resolves relative to retroarch.exe's own directory, and on
    // first run RetroArch looks for retroarch.cfg next to its own exe
    // before ever falling back to %APPDATA%, creating one there if none
    // exists. Writing this file explicitly is therefore belt-and-suspenders
    // rather than a workaround for missing portable support: it pins those
    // directories (and points libretro_directory at the same coresDir()
    // EmulatorProvider installs cores into) so Bili's "nothing outside its
    // own folder" guarantee doesn't silently depend on a future RetroArch
    // release keeping today's defaults.
    const QString dir = retroArchDir();
    QFile file(dir + "/retroarch.cfg");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    out << "system_directory = \"" << dir << "/system\"\n";
    out << "savefile_directory = \"" << dir << "/saves\"\n";
    out << "savestate_directory = \"" << dir << "/states\"\n";
    out << "screenshot_directory = \"" << dir << "/screenshots\"\n";
    out << "cache_directory = \"" << dir << "/cache\"\n";
    out << "libretro_directory = \"" << coresDir() << "\"\n";
}

bool EmulatorProvider::extractZipEntry(const QString &zipPath, const QString &entryFileName, const QString &destDir) {
    mz_zip_archive zipArchive;
    mz_zip_zero_struct(&zipArchive);
    if (!mz_zip_reader_init_file(&zipArchive, zipPath.toUtf8().constData(), 0)) {
        return false;
    }

    bool extracted = false;
    const mz_uint numFiles = mz_zip_reader_get_num_files(&zipArchive);
    for (mz_uint i = 0; i < numFiles; ++i) {
        if (mz_zip_reader_is_file_a_directory(&zipArchive, i)) continue;
        char nameBuf[1024];
        mz_zip_reader_get_filename(&zipArchive, i, nameBuf, sizeof(nameBuf));
        if (QString::fromUtf8(nameBuf) != entryFileName) continue;

        // entryFileName is matched above exactly as stored in the archive
        // (which may include subdirectory components for entries nested
        // inside the zip), but the extracted file is always written flat
        // into destDir under just its base name -- this generalizes the
        // helper (originally written for flat "<core>_libretro.dll"
        // entries) to arbitrary entry names, e.g. RomScanner's ROM entries,
        // without needing destDir's subdirectories to already exist.
        const QString destPath = destDir + "/" + QFileInfo(entryFileName).fileName();
        extracted = mz_zip_reader_extract_to_file(&zipArchive, i, destPath.toUtf8().constData(), 0);
        break;
    }

    mz_zip_reader_end(&zipArchive);
    return extracted;
}

void EmulatorProvider::persistInstalledState(const InstalledState &state) {
    QJsonObject cores;
    for (auto it = state.coresBySystem.begin(); it != state.coresBySystem.end(); ++it) {
        cores[it.key()] = it.value();
    }
    QJsonObject root;
    root["retroarch"] = state.retroArch;
    root["cores"] = cores;

    QDir().mkpath(QFileInfo(installedStatePath()).absolutePath());
    QFile file(installedStatePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
    }
}
