#pragma once
#include <QObject>
#include <QString>
#include <QMap>
#include <QSet>
#include <QUrl>
#include <QProcess>
#include <QTemporaryDir>
#include "EmulatorCatalog.h"
#include "GameWindowEmbedder.h"
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

    // Menu Bili en jeu (Task 4) : ouvre/ferme/quitte via l'interface de
    // commandes réseau de RetroArch (RetroArchNetworkCommand) plutôt que par
    // la fenêtre intégrée elle-même -- ni EmulatorProvider ni son QProcess
    // n'ont besoin de savoir que le jeu est en pause au niveau du process.
    //
    // No-op si aucun jeu n'est en cours (m_gameProcess nul ou déjà terminé).
    // No-op aussi si le menu est déjà ouvert (évite d'envoyer PAUSE_TOGGLE
    // deux fois, ce qui reprendrait immédiatement le jeu).
    Q_INVOKABLE void openGameMenu();

    // Termine le jeu en cours proprement : envoie "QUIT" et laisse à
    // RetroArch ~1s pour s'arrêter de lui-même avant de retomber sur le
    // pattern kill()+waitForFinished() déjà utilisé ailleurs dans cette
    // classe (voir le destructeur et l'échec d'embed() dans launchGame()).
    // N'émet jamais gameExited() elle-même : le handler QProcess::finished
    // déjà connecté dans launchGame() s'en charge inconditionnellement, quel
    // que soit le moyen par lequel le process se termine. No-op si aucun jeu
    // n'est en cours.
    Q_INVOKABLE void quitGame();

    // Referme le menu sans quitter le jeu : renvoie "PAUSE_TOGGLE" pour
    // reprendre l'exécution. No-op si le menu n'est pas ouvert.
    Q_INVOKABLE void resumeGame();

    Q_INVOKABLE bool isGameMenuOpen() const { return m_gameMenuOpen; }

    // Bascule le menu en jeu : l'ouvre s'il est fermé, le referme (sans
    // quitter) s'il est déjà ouvert -- demande explicite pour que
    // rappuyer sur Home (manette) ou Échap (clavier) reprenne le jeu au
    // lieu de rester sans effet (openGameMenu() seul est déjà un no-op si
    // le menu est ouvert). Partagé par les deux déclencheurs plutôt que
    // dupliqué : voir InputManager::homeMenuRequested (GamepadBridge.cpp)
    // et le hotkey Échap (app/main.cpp, EscapeHotkeyEventFilter).
    Q_INVOKABLE void toggleGameMenu() {
        if (isGameMenuOpen()) resumeGame();
        else openGameMenu();
    }

    // Whole-branch review fix: ui/Main.qml's screen-navigation guards need
    // to know "is a game actually running" independently of "is the in-game
    // menu open" -- isGameMenuOpen() alone made those guards implicitly
    // depend on QML Connections initialization order (see the review
    // report), and never covered "game running, menu closed" at all (e.g.
    // right after resumeGame(), before Fix 6's focus restore, keyboard input
    // could still reach Main.qml's own accept/navigate logic).
    Q_INVOKABLE bool isGameRunning() const { return m_gameProcess && m_gameProcess->state() != QProcess::NotRunning; }

    // Fenêtre hôte (celle de Bili) dans laquelle intégrer la fenêtre du jeu
    // au lancement -- fournie une fois par app/main.cpp (voir Task 3), qui
    // est le seul endroit du code ayant accès à la fois à QtQuick et à
    // EmulatorProvider. Un id de 0 (valeur par défaut) fait échouer tout
    // embed() -- launchGame() ne peut pas fonctionner tant que ce setter
    // n'a pas été appelé.
    void setHostWindowId(WId id) { m_hostWindowId = id; }

    // WId de la fenêtre du jeu actuellement intégrée, ou 0 si aucun jeu
    // n'est en cours. Lu depuis app/main.cpp pour donner à GameMenuOverlay
    // la fenêtre soeur au-dessus de laquelle placer le menu en jeu.
    WId gameWindowId() const { return m_windowEmbedder.embeddedWindowId(); }

    // Appelé depuis app/main.cpp (jamais depuis QML) quand la fenêtre hôte
    // change de taille, pour que la fenêtre du jeu actuellement intégrée
    // (s'il y en a une) suive. No-op si aucun jeu n'est en cours.
    void handleHostWindowResized() { m_windowEmbedder.resizeToHost(m_hostWindowId); }

    // Testing-only: réduit le budget de temps de GameWindowEmbedder::embed()
    // pour ne pas attendre les 5s réelles dans la suite automatisée.
    void setEmbedPollTimeoutForTesting(int ms) { m_windowEmbedder.setPollTimeoutForTesting(ms); }

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

    void gameMenuOpened();
    void gameMenuClosed();

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
    // Whole-branch review fix: writePortableRetroArchConfig() only ever runs
    // once, at RetroArch *install* time -- anyone who installed RetroArch
    // through Bili before the in-game menu feature existed has a
    // retroarch.cfg missing network_cmd_enable, so openGameMenu()'s
    // PAUSE_TOGGLE silently goes nowhere (RetroArchNetworkCommand::send() is
    // fire-and-forget, no delivery confirmation). Called from launchGame(),
    // every launch, before the process actually starts, so a freshly-started
    // RetroArch reads the migrated file at its own startup. Appends the two
    // missing lines rather than rewriting the whole file like
    // writePortableRetroArchConfig() does: RetroArch's own
    // config_save_on_exit may have already written real user settings
    // (button remaps, etc.) into this same file over time, and a full
    // rewrite would destroy them. No-op if the key is already present.
    void ensureNetworkCommandEnabled() const;
    // Bug fix + feature (manual testing, menu-en-jeu-bili): forces a small
    // set of RetroArch config values so its own native UI/chrome never
    // competes with Bili's own in-game menu:
    // - input_exit_emulator (Échap par défaut) et input_menu_toggle (F1 par
    //   défaut, github.com/libretro/RetroArch's own default retroarch.cfg)
    //   lisent la touche physique directement, indépendamment du hotkey
    //   global de Bili pour Échap (voir app/main.cpp,
    //   EscapeHotkeyEventFilter) -- le premier ouvrait/fermait le menu natif
    //   de RetroArch en même temps que celui de Bili, le second permettait
    //   d'ouvrir ce menu natif du tout. "nul" est la valeur documentée par
    //   RetroArch lui-même pour désactiver explicitement une touche.
    //   input_menu_toggle_gamepad_combo vaut déjà "0" (aucun) par défaut
    //   d'après cette même source, rien à désactiver côté manette.
    // - video_font_enable = false coupe tout affichage de texte à l'écran
    //   par RetroArch (nom du contenu/du core au chargement, compteur FPS,
    //   etc.) -- RetroArch n'a pas de bascule dédiée au seul message de
    //   chargement (confirmé : aucune clé "notification_show_*" de ce genre
    //   dans config.def.h, github.com/libretro/RetroArch).
    // - notification_show_autoconfig = false désactive spécifiquement le
    //   message "manette connectée/autoconfigurée" (confirmée réelle dans
    //   ce même config.def.h, vraie par défaut).
    // Unlike ensureNetworkCommandEnabled()'s simple presence check (a
    // boolean key genuinely means "already migrated" once present), all of
    // these keys can be PRESENT with their unwanted native value
    // (RetroArch's own config_save_on_exit would have written that
    // explicitly for anyone who played before one of these fixes, the exact
    // population each needs to reach) -- so this removes any existing line
    // for each key by pattern rather than just checking whether the key
    // exists, before appending the canonical desired lines.
    // Called from launchGame() alongside ensureNetworkCommandEnabled().
    void ensureRetroArchConfigOverrides() const;

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

    WId m_hostWindowId = 0;
    GameWindowEmbedder m_windowEmbedder;
    bool m_gameMenuOpen = false;

    static QString s_sevenZipPathOverride; // testing-only, see setSevenZipExecutablePathForTesting()
};
