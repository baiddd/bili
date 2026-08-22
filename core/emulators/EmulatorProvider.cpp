#include "EmulatorProvider.h"
#include "RetroArchAutoconfig.h"
#include "library/RomScanner.h"
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
#include <QRegularExpression>
#include <SDL.h>

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
        m_activeDownloadTempPaths.remove(requestId);
        m_activeTargets.remove(target);

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
        // Bug fix (final review, sub-project 3): this never removed the
        // pre-created empty temp file (see installCoreFrom()/
        // installRetroArchFrom()) on a failed download, the same class of
        // leak Task 2 already fixed once for EmulatorCatalog. Track
        // requestId -> tempPath alongside requestId -> target so it can be
        // cleaned up here too.
        const QString tempPath = m_activeDownloadTempPaths.take(requestId);
        if (!tempPath.isEmpty()) QFile::remove(tempPath);
        if (target.isEmpty()) return;
        m_activeTargets.remove(target);
        m_pendingCoreFilenames.remove(target);
        emit installFailed(target, errorString);
    });
}

EmulatorProvider::~EmulatorProvider() {
    // Bug fix (final review, sub-project 3): m_gameProcess is a QObject
    // child (`new QProcess(this)`), so Qt only destroys it as part of the
    // base QObject destructor, which runs *after* this destructor body
    // finishes. Without this, quitting the app while a game is still
    // running would delete m_gameTempDir (the extracted ROM's temp
    // directory) while RetroArch might still be reading from it. Terminate
    // the still-running game first and give it a moment to actually exit
    // before touching the temp dir it may be using.
    if (m_gameProcess && m_gameProcess->state() != QProcess::NotRunning) {
        m_gameProcess->kill();
        m_gameProcess->waitForFinished(3000);
    }
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

QString EmulatorProvider::gameTempDirPathForTesting() const {
    return m_gameTempDir ? m_gameTempDir->path() : QString();
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

QStringList EmulatorProvider::knownSystems() const {
    return RomScanner::knownSystems();
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
    // Re-entrancy guard (final review, sub-project 3): rapid double-clicking
    // "Installer" on the same row would otherwise map two request IDs to the
    // same target, racing installFinished/installFailed against each other.
    // Mirrors LibraryScanner::startScan()'s own m_scanning guard.
    if (m_activeTargets.contains(target)) return;

    QTemporaryFile temp;
    temp.setAutoRemove(false);
    temp.open();
    const QString tempPath = temp.fileName();
    temp.close();

    const int requestId = m_networkManager->startDownload(entry.url, tempPath);
    m_activeTargets.insert(target);
    m_activeDownloadTargets.insert(requestId, target);
    m_activeDownloadTempPaths.insert(requestId, tempPath);
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
    // Re-entrancy guard, see installCoreFrom()'s identical comment.
    if (m_activeTargets.contains("retroarch")) return;

    QTemporaryFile temp;
    temp.setAutoRemove(false);
    temp.open();
    const QString tempPath = temp.fileName();
    temp.close();

    const int requestId = m_networkManager->startDownload(url, tempPath);
    m_activeTargets.insert("retroarch");
    m_activeDownloadTargets.insert(requestId, "retroarch");
    m_activeDownloadTempPaths.insert(requestId, tempPath);
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

    ensureGamepadAutoconfig();

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
    connect(m_gameProcess, &QProcess::started, this, [this]() {
        // L'intégration de la fenêtre (Win32 SetParent) est requise avant de
        // considérer le lancement comme réussi -- un jeu qui tourne dans sa
        // propre fenêtre non intégrée n'est pas le comportement attendu
        // (voir docs/superpowers/specs/2026-08-22-fenetre-jeu-integree-design.md).
        // embed() est bloquant (jusqu'à ~5s dans le pire cas d'échec) ; le
        // process vient tout juste de démarrer, donc dans le cas normal la
        // fenêtre apparaît en quelques centaines de ms.
        if (!m_windowEmbedder.embed(m_gameProcess->processId(), m_hostWindowId)) {
            // Disconnect the finished handler below before killing the
            // process: kill()+waitForFinished() synchronously triggers it,
            // which would otherwise emit gameExited() (and the "game
            // launched, then exited" story that implies) for a launch that
            // never actually succeeded -- the same class of ambiguous-signal
            // bug the errorOccurred guard above already fixes for
            // errorOccurred(Crashed)+finished() both firing for one event.
            // Do that handler's cleanup (clearing m_gameTempDir) by hand
            // instead, since it won't run anymore.
            disconnect(m_gameProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, nullptr);
            m_gameProcess->kill();
            m_gameProcess->waitForFinished(3000);
            delete m_gameTempDir;
            m_gameTempDir = nullptr;
            emit launchFailed("Impossible d'intégrer la fenêtre du jeu.");
            return;
        }
        emit gameLaunched();
    });
    connect(m_gameProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int exitCode, QProcess::ExitStatus) {
        emit gameExited(exitCode);
        // Bug fix (Task 6 review): this was previously only done at the top
        // of the *next* launchGame() call (or in the destructor), so every
        // archived-ROM launch left its extracted temp dir sitting on disk
        // for the rest of the app session. Clean it up as soon as the game
        // that used it actually exits instead.
        delete m_gameTempDir;
        m_gameTempDir = nullptr;
    });
    connect(m_gameProcess, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
        // Bug fix (Task 6 review): QProcess emits errorOccurred() for
        // several distinct situations, not just "never started" -- notably,
        // when a process starts fine and later crashes, Qt emits BOTH
        // errorOccurred(QProcess::Crashed) and finished(exitCode,
        // QProcess::CrashExit) for that same event. Only FailedToStart means
        // RetroArch genuinely never launched; any other error (e.g. a
        // mid-game crash) is already correctly reported via the finished
        // handler above (gameExited carries the exit code), so emitting
        // launchFailed here too would wrongly tell the UI "launch failed"
        // about a game that demonstrably ran.
        if (error != QProcess::FailedToStart) return;
        emit launchFailed("Impossible de lancer RetroArch.");
    });

    m_gameProcess->start(retroArchExecutablePath(), launchArgs(corePath, resolvedRomPath));
}

QString EmulatorProvider::sevenZipExecutablePath() {
    if (!s_sevenZipPathOverride.isEmpty()) return s_sevenZipPathOverride;
    return QStandardPaths::findExecutable("7za", {QCoreApplication::applicationDirPath()});
}

void EmulatorProvider::setSevenZipExecutablePathForTesting(const QString &path) {
    s_sevenZipPathOverride = path;
}

// Removes a top-level entry of destDir by name, whether it's a file or a
// directory -- used by extract7zArchive() below to clean up exactly the
// entries a given extraction attempt created, never anything else.
static void removeTopLevelEntry(const QString &destDir, const QString &name) {
    const QString path = destDir + "/" + name;
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) return;
    if (info.isDir() && !info.isSymLink()) {
        QDir(path).removeRecursively();
    } else {
        QFile::remove(path);
    }
}

bool EmulatorProvider::extract7zArchive(const QString &archivePath, const QString &destDir) {
    const QString sevenZip = sevenZipExecutablePath();
    if (sevenZip.isEmpty()) return false;

    QDir().mkpath(destDir);

    // Bug fix (Critical, final review of sub-project 3): snapshot whatever
    // already lives directly under destDir *before* extraction runs. This
    // used to be skipped entirely, and the flatten/cleanup logic below
    // assumed destDir held nothing but this extraction's own output --
    // false whenever a core had already been installed first (which creates
    // destDir/cores/ before RetroArch is ever installed) or RetroArch was
    // already installed and is being reinstalled/upgraded (destDir already
    // has retroarch.exe et al. at its root). Root cause, live-reproduced:
    // with a pre-existing cores/ present, destDir ends up with TWO top-level
    // entries after extraction, the old "exactly one top-level dir" flatten
    // guard silently did nothing, retroarch.exe was never found, and the old
    // failure path's QDir(destDir).removeRecursively() wiped out cores/ (and
    // the core inside it) along with the empty extraction attempt. Tracking
    // exactly what predates this call lets both the flatten step and the
    // failure-cleanup step below touch only what this call itself produced.
    const QStringList preExistingEntries = QDir(destDir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    const QSet<QString> preExisting(preExistingEntries.begin(), preExistingEntries.end());

    QProcess process;
    process.start(sevenZip, {"x", archivePath, "-o" + destDir, "-y"});
    // Bug found during Task 8's manual verification: the real RetroArch.7z
    // (~194 MiB compressed, ~517 MiB / 14808 files uncompressed, confirmed
    // via a real download from buildbot.libretro.com) genuinely took ~76s to
    // extract via the vendored 7za.exe on ordinary dev hardware -- the
    // previous 60000ms timeout killed the extraction process before it could
    // finish, making every real RetroArch install fail with "Échec de
    // l'extraction de RetroArch." even though nothing was actually wrong.
    // 300000ms (5 minutes) leaves comfortable headroom above that measured
    // real-world time for slower disks/CPUs without waiting forever if 7za
    // is genuinely stuck.
    if (!process.waitForFinished(300000) || process.exitCode() != 0) {
        // Clean up only what THIS call may have partially created -- never
        // the whole destDir, which may hold pre-existing content (a prior
        // core install, or the old RetroArch install being upgraded) that
        // has nothing to do with this failed attempt.
        const QStringList afterEntries = QDir(destDir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        for (const QString &entry : afterEntries) {
            if (!preExisting.contains(entry)) removeTopLevelEntry(destDir, entry);
        }
        return false;
    }

    const QStringList afterEntries = QDir(destDir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    QStringList newEntries;
    for (const QString &entry : afterEntries) {
        if (!preExisting.contains(entry)) newEntries.append(entry);
    }

    // The official RetroArch.7z (verified against a real download from
    // buildbot.libretro.com/stable/1.22.2/windows/x86_64/RetroArch.7z) wraps
    // its entire contents in a single top-level folder ("RetroArch-Win64/")
    // rather than placing retroarch.exe at the archive root, and 7-Zip's "x"
    // command has no "strip leading path component" switch the way tar
    // does. Search only among this extraction's OWN new entries (not every
    // top-level entry in destDir, which may include unrelated pre-existing
    // content) for the one that actually contains retroarch.exe -- a real
    // archive should only ever produce one such entry, but this doesn't
    // assume that: zero matches is a genuine failure, and more than one
    // (shouldn't happen, but don't crash if it somehow does) just picks the
    // first and is otherwise harmless. A no-op search for the test's fake
    // 7za.exe stand-in, which writes retroarch.exe straight into destDir
    // with no nesting (it shows up directly in newEntries as a file, not a
    // matched subdirectory).
    QString matchedSubdir;
    for (const QString &entry : newEntries) {
        const QString candidate = destDir + "/" + entry;
        if (!QFileInfo(candidate).isDir()) continue;
        if (!QFile::exists(candidate + "/retroarch.exe")) continue;
        if (matchedSubdir.isEmpty()) {
            matchedSubdir = candidate;
        } else {
            qWarning("EmulatorProvider: extraction produced more than one new "
                     "subdirectory containing retroarch.exe under %s; using %s",
                     qUtf8Printable(destDir), qUtf8Printable(matchedSubdir));
        }
    }

    if (!matchedSubdir.isEmpty()) {
        const QStringList items = QDir(matchedSubdir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);

        // Defensive guard (re-review of the final fix wave, sub-project 3):
        // the merge below overwrites-by-name any top-level entry of
        // matchedSubdir against destDir, with no distinction between "the
        // archive's own content" and content Bili itself owns -- namely
        // coresDir()'s basename ("cores"), which lives directly under this
        // same destDir. Not currently triggered by the real archive
        // (RetroArch.7z and RetroArch_cores.7z are confirmed-separate
        // archives on the real buildbot), but if a future RetroArch.7z ever
        // ships a top-level cores/ entry, an unguarded merge would silently
        // destroy every installed core -- the exact same bug class as the
        // Critical bug this function's own header comment already fixed one
        // level up (a pre-existing coresDir() colliding with extraction),
        // just reached via the merge step instead of the failure-cleanup
        // step. This is out of this project's control (a future upstream
        // archive layout change), so guard against it now rather than wait
        // for it to happen. If found, skip the merge entirely (leave
        // matchedSubdir intact, don't rmdir it) and fall through to the
        // retroarch.exe existence check below, which will correctly fail
        // and clean up only this extraction attempt's own new entries.
        const QString coresDirName = QFileInfo(coresDir()).fileName();
        bool collidesWithCoresDir = false;
        for (const QString &item : items) {
            if (item.compare(coresDirName, Qt::CaseInsensitive) == 0) {
                collidesWithCoresDir = true;
                break;
            }
        }

        if (collidesWithCoresDir) {
            qWarning("EmulatorProvider: refusing to merge %s into %s -- it "
                     "contains a top-level entry named '%s', which collides "
                     "with Bili's own cores directory; aborting this "
                     "extraction rather than risk overwriting installed cores",
                     qUtf8Printable(matchedSubdir), qUtf8Printable(destDir),
                     qUtf8Printable(coresDirName));
        } else {
            // Move (overwriting, not a plain rename that fails outright if
            // the destination already exists) each item up into destDir,
            // then remove the now-empty subdirectory. Overwriting is what
            // makes an upgrade reinstall work correctly: an old
            // retroarch.exe (and old DLLs etc.) already sitting at destDir's
            // root gets replaced by the newly-extracted ones instead of the
            // new files being silently ignored in a leftover nested folder.
            for (const QString &item : items) {
                const QString from = matchedSubdir + "/" + item;
                const QString to = destDir + "/" + item;
                if (QFileInfo::exists(to) || QFileInfo(to).isSymLink()) {
                    removeTopLevelEntry(destDir, item);
                }
                QDir().rename(from, to);
            }
            QDir().rmdir(matchedSubdir);
        }
    }

    // Only report success once retroarch.exe genuinely landed at the
    // expected path. Without this check, either (a) no new entry containing
    // retroarch.exe was found at all, or (b) the move above somehow still
    // didn't land it (e.g. a locked file preventing the final rename) would
    // silently be treated as a successful install by the caller, which
    // persists installed.json and deletes the downloaded archive --
    // destroying the only way to retry without re-downloading. Clean up
    // only the NEW entries this call created on failure (never
    // removeRecursively() the whole destDir -- see this method's opening
    // comment for why that destroys pre-existing content) so a later retry
    // doesn't find debris left behind here, while pre-existing content
    // (e.g. an already-installed core's cores/) is left completely alone.
    if (!QFile::exists(destDir + "/retroarch.exe")) {
        for (const QString &entry : newEntries) {
            removeTopLevelEntry(destDir, entry);
        }
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

    // Research (final review, sub-project 3): RetroArch's own input/
    // controller-driver reference (docs.libretro.com/guides/
    // input-controller-drivers/) confirms "sdl2" is a real, documented
    // input_joypad_driver value on Windows (alongside dinput/xinput/hid) --
    // not a guess. RetroArch's default joypad driver on Windows is
    // "xinput", a completely different backend from the "sdl2" numbering
    // RetroArchAutoconfig's generated profiles assume (they set
    // input_driver = "sdl2" inside the profile itself, per Task 7). Left at
    // the xinput default, a generated autoconfig profile's button/axis
    // numbers would be interpreted against the wrong backend's numbering.
    // Pinning the joypad driver to match fixes that mismatch; verifying the
    // actual button response still needs real hardware + a real RetroArch
    // run, which is on the user to check per this project's convention.
    out << "input_joypad_driver = \"sdl2\"\n";

    // Feature (manual testing follow-up): RetroArch ships with no gamepad
    // binding at all for its "quit" hotkey by default -- input_exit_emulator
    // is bound to the Escape key only (confirmed against RetroArch's own
    // default retroarch.cfg, github.com/libretro/RetroArch). Without this,
    // a controller-only user has no way to exit a running game. RetroArch
    // exposes a purpose-built combo mechanism for exactly this
    // (input_quit_gamepad_combo, an enum of predefined combos -- NOT a
    // single-button _btn bind, so no per-controller button-index mapping is
    // needed here). Value 4 is "Start + Select", the combo listed in
    // RetroArch's own default config comment and a convention going back to
    // classic consoles, chosen over "hold Select 2s" (value 8) for being
    // more universally recognized/discoverable by players on first try.
    out << "input_quit_gamepad_combo = \"4\"\n";
}

void EmulatorProvider::ensureGamepadAutoconfig() {
    // Research (Task 7): GamepadBridge opens every connected controller
    // (SDL_GameControllerOpen) on its own dedicated poll QThread and never
    // exposes the resulting SDL_GameController* handles -- this method runs
    // on the GUI thread instead, so it must never touch those handles.
    // Instead it uses SDL2's device-index-based query functions
    // (SDL_NumJoysticks/SDL_IsGameController/SDL_GameControllerNameForIndex/
    // SDL_GameControllerMappingForDeviceIndex), which query a connected
    // device by its enumeration index without requiring it to already be
    // open.
    //
    // Cross-thread safety of those specific functions was verified against
    // SDL2's own documentation rather than assumed: their individual wiki
    // pages (wiki.libsdl.org/SDL2/SDL_GameControllerNameForIndex etc.) don't
    // document thread-safety at all, but SDL_joystick.h's own
    // SDL_LockJoysticks()/SDL_UnlockJoysticks() documentation directly
    // addresses this exact scenario: "If you are using the joystick API or
    // handling events from multiple threads you should use these locking
    // functions to protect access to the joysticks. In particular, you are
    // guaranteed that the joystick list won't change, so the API functions
    // that take a joystick index will be valid, and joystick and game
    // controller events will not be delivered [while held]." That covers
    // exactly the four calls below and GamepadBridge's own event-driven
    // hotplug handling on its poll thread, so this wraps them accordingly
    // instead of calling them bare.
    //
    // Guard with SDL_WasInit() first: SDL only creates the mutex
    // SDL_LockJoysticks() locks once SDL_JoystickInit() has actually run
    // (i.e. once GamepadBridge's poll thread has called
    // SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK)), and
    // this method can otherwise run before that -- e.g. in tests, which
    // never start GamepadBridge at all -- in which case there are, by
    // definition, no controllers to enumerate yet either way.
    if (!(SDL_WasInit(SDL_INIT_GAMECONTROLLER) & SDL_INIT_GAMECONTROLLER)) return;

    SDL_LockJoysticks();
    const int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; ++i) {
        if (!SDL_IsGameController(i)) continue;

        const char *namePtr = SDL_GameControllerNameForIndex(i);
        char *mappingPtr = SDL_GameControllerMappingForDeviceIndex(i);
        const QString name = namePtr ? QString::fromUtf8(namePtr) : QString();
        const QString mapping = mappingPtr ? QString::fromUtf8(mappingPtr) : QString();
        if (mappingPtr) SDL_free(mappingPtr); // "Must be freed with SDL_free()" per SDL_gamecontroller.h
        if (name.isEmpty() || mapping.isEmpty()) continue;

        // Sanitize the controller name into a safe filename (RetroArch
        // matches autoconfig files by their contents, not their filename,
        // but the filename still needs to be filesystem-safe).
        QString safeName = name;
        safeName.replace(QRegularExpression("[^A-Za-z0-9 _-]"), "_");
        const QString path = retroArchDir() + "/autoconfig/" + safeName + ".cfg";
        if (QFile::exists(path)) continue;

        QDir().mkpath(retroArchDir() + "/autoconfig");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(RetroArchAutoconfig::buildProfile(name, mapping).toUtf8());
        }
    }
    SDL_UnlockJoysticks();
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
