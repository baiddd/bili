#pragma once
#include <QObject>
#include <QString>
#include <QMap>
#include <QSet>
#include <QUrl>
#include <QProcess>
#include <QTemporaryDir>
#include "EmulatorCatalog.h"
#include "network/NetworkManager.h"

class EmulatorProvider : public QObject {
    Q_OBJECT
public:
    explicit EmulatorProvider(QString dataDir, NetworkManager *networkManager, QObject *parent = nullptr);
    ~EmulatorProvider() override;

    Q_INVOKABLE bool isRetroArchInstalled() const;
    Q_INVOKABLE bool isCoreInstalled(const QString &system) const;

    // Populated once main.cpp's EmulatorCatalog::ready fires (Task 8) —
    // wires the live catalog::emulators.json manifest into this provider so
    // installCore()/installRetroArch() can resolve real download URLs.
    void setCatalogData(const EmulatorCatalogData &data) { m_catalogData = data; }

    // Forwards RomScanner::knownSystems() (Task 1) so QML can build a
    // "one row per known system" list without RomScanner itself being a
    // QObject exposed to QML.
    Q_INVOKABLE QStringList knownSystems() const;

    Q_INVOKABLE void installCore(const QString &system);
    // Testing-only entry point that skips the catalog lookup (EmulatorCatalogTest
    // already covers catalog parsing; this lets EmulatorProviderTest exercise
    // download+extract+state-recording in isolation).
    void installCoreFrom(const QString &system, const CoreCatalogEntry &entry);

    Q_INVOKABLE void installRetroArch();
    // Testing-only entry point, mirrors installCoreFrom.
    void installRetroArchFrom(const QUrl &url);

    Q_INVOKABLE void uninstallRetroArch();
    Q_INVOKABLE void uninstallCore(const QString &system);

    // Resolves the installed core for `system`, builds a retroarch.exe
    // command line via launchArgs(), and runs it through a (non-detached)
    // QProcess so gameExited(int) fires when the game process quits.
    Q_INVOKABLE void launchGame(const QString &romPath, const QString &system);
    // Exposed for testing: builds the retroarch.exe argument list without
    // ever actually starting a process, same pattern as
    // SystemController::restartArgs()/shutdownArgs() from the socle.
    static QStringList launchArgs(const QString &corePath, const QString &resolvedRomPath);

    // Exposed for testing: where this class looks for 7za.exe. Searches only
    // applicationDirPath() - there is no PATH fallback, since
    // QStandardPaths::findExecutable() with a non-empty `paths` argument
    // searches *only* those paths and never also falls back to $PATH (proven
    // during Task 8's manual verification: dev-env.ps1 putting
    // platform/windows/tools on PATH never helped this lookup at all). Both
    // the packaged dist build and a plain dev build instead need 7za.exe
    // physically copied next to Bili.exe: publish_windows.ps1 does this for
    // dist, and app/CMakeLists.txt's POST_BUILD step does it for a plain
    // `cmake --build`.
    static QString sevenZipExecutablePath();
    // Testing-only override for sevenZipExecutablePath(), so tests can point
    // it at a fake stand-in instead of a real vendored/PATH-resolved 7za.exe.
    // Pass an empty string to restore normal resolution.
    static void setSevenZipExecutablePathForTesting(const QString &path);

    // Exposed for testing: the exact paths this class checks/writes to,
    // without touching the filesystem.
    QString retroArchDir() const;         // "<dataDir>/emulators/retroarch"
    QString retroArchExecutablePath() const; // "<retroArchDir>/retroarch.exe"
    QString coresDir() const;             // "<retroArchDir>/cores"
    QString installedStatePath() const;   // "<dataDir>/emulators/installed.json"

    // Exposed for testing: the temp directory currently holding a ROM
    // extracted from a "<archive>::<entry>" rom_path for the running game
    // (see launchGame()), or an empty string if there is none. Lets tests
    // confirm it gets cleaned up once the game process exits.
    QString gameTempDirPathForTesting() const;

signals:
    void installProgress(const QString &target, qint64 bytesReceived, qint64 bytesTotal);
    void installFinished(const QString &target);
    void installFailed(const QString &target, const QString &errorString);
    void uninstallFinished(const QString &target);
    void uninstallFailed(const QString &target, const QString &errorString);

    void gameLaunched();
    void gameExited(int exitCode);
    void launchFailed(const QString &errorString);

protected:
    // Reads installed.json into memory; returns an empty/default state if
    // the file doesn't exist or fails to parse (never treated as an
    // error - "no state file yet" is the normal state on first run).
    struct InstalledState {
        bool retroArch = false;
        QMap<QString, QString> coresBySystem; // system -> core name
    };
    InstalledState readInstalledState() const;

    QString m_dataDir;

private:
    // Read-modify-write helper used by every install/uninstall path (Tasks
    // 3-5): callers read the current state, mutate the in-memory copy, and
    // call this once to persist it — a single place that builds/writes the
    // JSON, so no call site duplicates that logic.
    void persistInstalledState(const InstalledState &state);
    bool extractZipEntry(const QString &zipPath, const QString &entryFileName, const QString &destDir);
    bool extract7zArchive(const QString &archivePath, const QString &destDir);
    // Writes a default RetroArch autoconfig .cfg (via RetroArchAutoconfig)
    // for every currently-connected gamepad that doesn't already have one,
    // so a first launch with a controller plugged in gets working input
    // without the user configuring it by hand. Called from launchGame()
    // once RetroArch is confirmed installed. See RetroArchAutoconfig.h and
    // this method's own definition for the SDL2 cross-thread research this
    // is built on (GamepadBridge owns SDL's joystick/game-controller
    // subsystem on its own poll thread; this runs on the GUI thread).
    void ensureGamepadAutoconfig();
    // Writes a retroarch.cfg pinning system/save/state/cache/core directories
    // under retroArchDir(), so Bili's "nothing outside its own folder"
    // guarantee doesn't depend on RetroArch's own (already portable-by-default
    // on Windows) directory defaults staying that way in a future release.
    void writePortableRetroArchConfig() const;

    NetworkManager *m_networkManager;
    EmulatorCatalogData m_catalogData; // populated via setCatalogData() once main.cpp's EmulatorCatalog::ready fires (Task 8)
    QMap<int, QString> m_activeDownloadTargets; // requestId -> "core:<system>" / "retroarch"
    // requestId -> the temp file startDownload() writes into for that request.
    // NetworkManager::failed only carries the requestId/errorString, not the
    // path, so this is needed to remove the right pre-created empty temp file
    // once a download fails (fix for the leak Task 2 already fixed once for
    // EmulatorCatalog and predicted, then confirmed, to recur here).
    QMap<int, QString> m_activeDownloadTempPaths;
    QMap<QString, QString> m_pendingCoreFilenames; // target -> expected "<core>_libretro.dll" once known
    // Re-entrancy guard: rapid double-clicking "Installer" on the same row
    // (EmulatorManagerScreen.qml) would otherwise start two downloads for the
    // same target, racing installFinished/installFailed against each other.
    // Mirrors LibraryScanner::startScan()'s own m_scanning guard.
    QSet<QString> m_activeTargets;

    QProcess *m_gameProcess = nullptr;
    // Holds the temp file extracted from a "<archive>::<entry>" rom_path
    // (RomScanner's convention for content found inside a .zip) for the
    // duration of the current game process -- see launchGame()'s comment
    // for why extraction, not a RetroArch command-line archive syntax, is
    // used. Owns the temp directory (and thus deletes the extracted file)
    // once replaced or the provider is destroyed.
    QTemporaryDir *m_gameTempDir = nullptr;

    static QString s_sevenZipPathOverride; // testing-only, see setSevenZipExecutablePathForTesting()
};
