#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>
#include <QWindow>

#ifdef Q_OS_WIN
#include <QAbstractNativeEventFilter>
#include <QQuickView>
#include <QMap>
#include <functional>
#include <windows.h>
#include "GameMenuOverlay.h"
#include "GameFrameImageProvider.h"
#endif

#include "input/GamepadBridge.h"
#include "input/InputManager.h"
#include "storage/ConfigStore.h"
#include "storage/RomSourcesStore.h"
#include "storage/LibraryDatabase.h"
#include "library/LibraryModel.h"
#include "library/LibraryScanner.h"
#include "network/NetworkManager.h"
#include "ui/ScreenManager.h"
#include "system/SystemController.h"
#include "emulators/EmulatorProvider.h"
#include "emulators/EmulatorCatalog.h"
#include "scraper/StubScraperProvider.h"
#include "scraper/ScraperProviderQmlBridge.h"
#include "netplay/StubNetplaySession.h"
#include "netplay/NetplaySessionQmlBridge.h"

#ifdef Q_OS_WIN
// Relaie WM_SIZE de la fenêtre racine vers EmulatorProvider, pour que la
// fenêtre du jeu actuellement intégrée (s'il y en a une) suive un
// redimensionnement de la fenêtre de Bili. handleHostWindowResized() est
// un no-op si aucun jeu n'est en cours -- ce filtre peut donc tourner en
// permanence sans se soucier de l'état courant.
class HostResizeEventFilter : public QAbstractNativeEventFilter {
public:
    explicit HostResizeEventFilter(EmulatorProvider *provider) : m_provider(provider) {}

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) override {
        if (eventType != "windows_generic_MSG") return false;
        auto *msg = static_cast<MSG *>(message);
        if (msg->message == WM_SIZE) {
            m_provider->handleHostWindowResized();
        }
        return false; // ne consomme jamais l'évènement -- Qt doit le traiter aussi
    }

private:
    EmulatorProvider *m_provider;
};

// Suit resizeToHost() -- déclenché par un redimensionnement de la fenêtre
// hôte PENDANT que le menu en jeu est ouvert -- avec la même
// resynchronisation manuelle de la géométrie Qt que le premier show()
// (`syncGeometry`, voir sa définition dans main() : Qt ne retraduit pas en
// évènement de resize le MoveWindow natif que GameMenuOverlay fait sur son
// propre HWND).
//
// Contrairement à HostResizeEventFilter ci-dessus, filtre explicitement sur
// le HWND de l'HÔTE : show() lui-même déclenche des WM_SIZE/WM_MOVE
// synchrones sur le HWND du MENU (SetParent/MoveWindow à l'intérieur de
// GameMenuOverlay::show()), et m_provider->isGameMenuOpen() est déjà vrai à
// ce moment-là (posé par EmulatorProvider::openGameMenu() avant d'émettre
// gameMenuOpened(), donc avant que show() ne soit même appelé) -- sans ce
// filtrage par HWND, ce filtre se déclencherait en RÉENTRANCE pendant show()
// lui-même et rappellerait resizeToHost() sur une fenêtre à moitié
// configurée (m_overlayWindowId pas encore posé), corrompant l'affichage.
// Constaté à la vérification manuelle de cette tâche : la première ouverture
// du menu échouait à afficher quoi que ce soit une fois ce filtre ajouté,
// jusqu'à ce que ce filtrage par HWND soit ajouté.
class MenuResizeEventFilter : public QAbstractNativeEventFilter {
public:
    MenuResizeEventFilter(EmulatorProvider *provider, GameMenuOverlay *overlay, WId hostWindowId,
                           std::function<void()> syncGeometry)
        : m_provider(provider), m_overlay(overlay), m_hostWindowId(hostWindowId),
          m_syncGeometry(std::move(syncGeometry)) {}

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) override {
        if (eventType != "windows_generic_MSG") return false;
        auto *msg = static_cast<MSG *>(message);
        if (msg->message == WM_SIZE && msg->hwnd == reinterpret_cast<HWND>(m_hostWindowId)
            && m_provider->isGameMenuOpen()) {
            m_overlay->resizeToHost(m_hostWindowId);
            m_syncGeometry();
        }
        return false;
    }

private:
    EmulatorProvider *m_provider;
    GameMenuOverlay *m_overlay;
    WId m_hostWindowId;
    std::function<void()> m_syncGeometry;
};

// Identifiants arbitraires (juste besoin d'être uniques parmi les hotkeys
// que CE process enregistre) pour les touches enregistrées comme hotkeys
// système via RegisterHotKey() ci-dessous.
constexpr int kEscapeHotkeyId = 1;
constexpr int kEnterHotkeyId = 2;

// Demande explicite : Échap au clavier doit ouvrir/fermer le menu en jeu, et
// Entrée doit y déclencher "Quitter le jeu" -- comme le bouton Home/Guide et
// le bouton A/Croix le font déjà à la manette (GamepadBridge.cpp). Mais
// pendant qu'un jeu tourne (Échap) OU que le menu est affiché (Entrée),
// c'est la fenêtre de RetroArch OU celle du menu en jeu qui devrait avoir le
// vrai focus clavier Windows -- SDL2 lit la manette indépendamment du focus
// (thread de poll dédié dans GamepadBridge), mais le clavier suit les
// règles normales de focus de l'OS, et RetroArch reprend parfois ce focus
// de lui-même après une pause (constaté en vérification manuelle) --
// rendant Entrée peu fiable sur le bouton QML même une fois le menu
// affiché. RegisterHotKey() est la façon standard Win32 de recevoir une
// touche globalement, peu importe quelle fenêtre (de ce process ou d'un
// autre) a le focus -- WM_HOTKEY est alors livré à la fenêtre qui a
// enregistré le hotkey, ici la fenêtre racine de Bili, via la boucle de
// messages normale (donc visible par un QAbstractNativeEventFilter comme
// les autres filtres de ce fichier). Échap est enregistré seulement pendant
// qu'un jeu tourne (gameLaunched/gameExited plus bas dans main()) ; Entrée
// seulement pendant que le menu est affiché (gameMenuOpened/gameMenuClosed)
// -- pour ne jamais intercepter ces touches globalement sur le reste du
// système en dehors de ces contextes précis.
class MenuHotkeyEventFilter : public QAbstractNativeEventFilter {
public:
    explicit MenuHotkeyEventFilter(EmulatorProvider *provider) : m_provider(provider) {}

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) override {
        if (eventType != "windows_generic_MSG") return false;
        auto *msg = static_cast<MSG *>(message);
        if (msg->message != WM_HOTKEY) return false;

        // Anti-rebond -- constaté empiriquement via un log de diagnostic
        // temporaire pendant la vérification manuelle de cette
        // fonctionnalité : Windows livre DEUX WM_HOTKEY pour un seul appui
        // physique, ~50-70ms d'écart, de façon parfaitement systématique
        // (pas un double-appui accidentel de l'utilisateur -- l'écart était
        // trop régulier). MOD_NOREPEAT (déjà passé à RegisterHotKey pour
        // chaque touche) empêche la répétition tant que la touche reste
        // enfoncée, mais pas ce doublon-ci -- cause exacte non identifiée
        // (potentiellement une particularité de la livraison de WM_HOTKEY
        // par la boucle d'évènements native de Qt), mais ignorer tout
        // WM_HOTKEY du même identifiant reçu moins de kDebounceMs après le
        // précédent neutralise le symptôme de façon fiable indépendamment
        // de cette cause. Un debounce séparé par identifiant (pas un seul
        // partagé) pour qu'appuyer sur l'une des deux touches juste après
        // l'autre reste réactif.
        const qint64 now = static_cast<qint64>(GetTickCount64());
        qint64 &lastHandledMs = m_lastHandledMsByHotkeyId[static_cast<int>(msg->wParam)];
        if (now - lastHandledMs < kDebounceMs) return false;
        lastHandledMs = now;

        if (msg->wParam == kEscapeHotkeyId) {
            // toggleGameMenu() (pas openGameMenu()) : rappuyer sur Échap
            // pendant que le menu est déjà ouvert doit reprendre le jeu,
            // pas rester sans effet -- demande explicite, symétrique avec
            // le bouton Home de la manette (voir GamepadBridge.cpp).
            m_provider->toggleGameMenu();
        } else if (msg->wParam == kEnterHotkeyId) {
            // Seule action du menu en jeu en V1 -- pas besoin de router vers
            // le QML (qui ne recevrait de toute façon pas l'évènement de
            // façon fiable, voir le commentaire de cette classe).
            m_provider->quitGame();
        }
        return false;
    }

private:
    static constexpr qint64 kDebounceMs = 300;
    QMap<int, qint64> m_lastHandledMsByHotkeyId;
    EmulatorProvider *m_provider;
};
#endif

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    ScreenManager screenManager;
    engine.rootContext()->setContextProperty("ScreenManager", &screenManager);

    InputManager inputManager;
    engine.rootContext()->setContextProperty("InputManager", &inputManager);

    const QString dataDir = QCoreApplication::applicationDirPath() + "/data";
    ConfigStore configStore(dataDir);
    configStore.load();
    RomSourcesStore romSourcesStore(&configStore);
    engine.rootContext()->setContextProperty("RomSourcesStore", &romSourcesStore);
    engine.rootContext()->setContextProperty("applicationDirPath", QCoreApplication::applicationDirPath());

    // LibraryDatabase (SQLite) can't create its file inside a directory that
    // doesn't exist yet - ConfigStore only creates <dataDir> lazily on its
    // own first save(), so ensure the portable data folder exists here too.
    QDir().mkpath(dataDir);
    LibraryDatabase libraryDb(dataDir + "/library.db");
    libraryDb.open();

    // libraryModel/libraryScanner hold raw LibraryDatabase* with no ownership,
    // so they must be declared after libraryDb here so C++ destroys them
    // (in reverse declaration order) before libraryDb at scope exit.
    LibraryModel libraryModel(&libraryDb);
    engine.rootContext()->setContextProperty("LibraryModel", &libraryModel);

    LibraryScanner libraryScanner(&libraryDb);
    engine.rootContext()->setContextProperty("LibraryScanner", &libraryScanner);

    QObject::connect(&libraryScanner, &LibraryScanner::scanFinished,
                      &libraryModel, &LibraryModel::refresh);

    NetworkManager networkManager;
    engine.rootContext()->setContextProperty("NetworkManager", &networkManager);

    GamepadBridge gamepadBridge(&inputManager);
    gamepadBridge.start();
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                      [&gamepadBridge]() { gamepadBridge.stop(); });

    // Must run to completion before main() returns and starts destroying
    // libraryScanner/libraryDb: without this, a scan still running when the
    // window closes leaves its QtConcurrent::run worker with nothing
    // canceling or waiting for it, so Qt's global QThreadPool static
    // destructor blocks the whole process at exit until that worker finishes
    // on its own (minutes, for a large source) - and the worker's lambda
    // would otherwise go on touching a LibraryScanner/LibraryDatabase that
    // may already be destroyed. aboutToQuit fires synchronously from within
    // app.exec()'s own shutdown sequence, before it returns, so this is
    // guaranteed to complete before the objects below go out of scope.
    QObject::connect(&app, &QCoreApplication::aboutToQuit,
                      [&libraryScanner]() { libraryScanner.cancelAndWait(); });

    EmulatorProvider emulatorProvider(dataDir, &networkManager);
    engine.rootContext()->setContextProperty("EmulatorProvider", &emulatorProvider);

#ifdef Q_OS_WIN
    HostResizeEventFilter resizeFilter(&emulatorProvider);
    app.installNativeEventFilter(&resizeFilter);
#endif

    EmulatorCatalog emulatorCatalog(&networkManager);
    engine.rootContext()->setContextProperty("EmulatorCatalog", &emulatorCatalog);
    QObject::connect(&emulatorCatalog, &EmulatorCatalog::ready,
                      [&emulatorProvider](const EmulatorCatalogData &data) {
        emulatorProvider.setCatalogData(data);
    });
    // Boot-time fetch gives a head start before the user ever opens
    // EmulatorManagerScreen; that screen also calls EmulatorCatalog.fetch()
    // itself on Component.onCompleted (see EmulatorManagerScreen.qml) so
    // re-opening it after a failed/offline fetch is a genuine retry path,
    // not just a one-shot attempt at boot (fix wave, sub-project 3 final
    // review - this was a regression against the original design spec).
    emulatorCatalog.fetch(EmulatorCatalog::manifestUrl());

    StubScraperProvider scraperProvider;
    ScraperProviderQmlBridge scraperBridge(&scraperProvider);
    engine.rootContext()->setContextProperty("ScraperProvider", &scraperBridge);

    StubNetplaySession netplaySession;
    NetplaySessionQmlBridge netplayBridge(&netplaySession);
    engine.rootContext()->setContextProperty("NetplaySession", &netplayBridge);

    SystemController systemController;
    engine.rootContext()->setContextProperty("SystemController", &systemController);

    engine.loadFromModule("Bili", "Main");

    // engine.rootObjects() n'est peuplé qu'après loadFromModule() -- il n'y
    // a donc pas d'ordre alternatif possible pour cette étape. root est la
    // fenêtre ApplicationWindow de ui/Main.qml elle-même (un QQuickWindow,
    // sous-classe de QWindow) : winId() donne son HWND natif Windows.
    QWindow *rootWindow = qobject_cast<QWindow *>(engine.rootObjects().value(0));
    if (rootWindow) {
        emulatorProvider.setHostWindowId(rootWindow->winId());
    }

#ifdef Q_OS_WIN
    // Menu Bili en jeu (Task 4). Deuxième QQuickWindow partageant le MÊME
    // QQmlEngine que la fenêtre principale -- QQuickView(engine, parent) ne
    // crée pas de moteur privé, et QQuickView::rootContext() n'est qu'un
    // raccourci vers engine->rootContext() dans ce cas (vérifié dans les
    // sources Qt : QQuickComponent::create() sans contexte explicite utilise
    // le rootContext() du moteur). Theme (singleton QML du module Bili),
    // EmulatorProvider et InputManager, déjà exposés plus haut sur
    // engine.rootContext(), sont donc directement visibles depuis
    // InGameMenuOverlay.qml sans rien réexposer pour eux.
    GameFrameImageProvider *gameFrameProvider = new GameFrameImageProvider();
    engine.addImageProvider("gameframe", gameFrameProvider); // l'engine en prend possession

    GameMenuOverlay gameMenuOverlay;
    gameMenuOverlay.setFrameProvider(gameFrameProvider);
    engine.rootContext()->setContextProperty("GameMenuOverlay", &gameMenuOverlay);

    QQuickView menuView(&engine, nullptr);
    // Dimensionne la fenêtre à la taille du panneau (480x260, l'implicitWidth/
    // implicitHeight d'InGameMenuOverlay.qml) AVANT le premier show() --
    // GameMenuOverlay la mémorise à cet instant pour son repli
    // Fit::CenteredPanel (voir GameMenuOverlay.h).
    // NB: le préfixe de ressource réel d'un module QML généré par
    // qt_add_qml_module (Qt 6.5+) est "qt/qml/<Module>/", pas "<Module>/" --
    // confirmé en inspectant Bili_raw_qml_0.qrc généré par CMake après un
    // premier essai à "qrc:/Bili/screens/..." qui échouait silencieusement
    // ("No such file or directory", visible seulement via
    // QQuickView::errors(), voir la vérification manuelle de cette tâche).
    // engine.loadFromModule("Bili", "Main") plus haut n'a pas ce problème
    // car il résout via le système de modules (qmldir), pas un chemin de
    // ressource brut.
    menuView.setSource(QUrl(QStringLiteral("qrc:/qt/qml/Bili/screens/InGameMenuOverlay.qml")));
    menuView.resize(480, 260);
    // Whole-branch review fix: QQuickView's actual default resizeMode is
    // SizeViewToRootObject (a previous comment here incorrectly claimed the
    // default was SizeRootObjectToView) -- i.e. by default the WINDOW's size
    // follows the ROOT ITEM's size, the opposite of what's needed here.
    // GameMenuOverlay::show()/resizeToHost() resize menuView's real HWND in
    // Win32 pur (SetParent/MoveWindow, see GameMenuOverlay.cpp) and Qt does
    // not retranslate that native resize into its own resize-event pipeline
    // for a window it created but saw restyled/reparented outside its
    // control -- so the caller must explicitly push the real geometry back
    // into Qt afterwards (syncMenuViewGeometryFromNativeWindow() below).
    // Explicitly requesting SizeRootObjectToView here means that manual
    // resize() call is enough on its own to also resize the QML root Item
    // (the veil + captured-frame Image, both anchors.fill: parent) --
    // without it, only the window's own QWindow::width()/height() would
    // update, while the root Item stayed stuck at whatever size it last
    // had (480x260), which is why the previous version of this code also
    // had to call QQuickItem::setWidth()/setHeight() by hand.
    menuView.setResizeMode(QQuickView::SizeRootObjectToView);

    // Resynchronise la géométrie Qt de menuView avec la géométrie RÉELLE de
    // son HWND après tout redimensionnement natif fait par GameMenuOverlay
    // (le premier show(), et tout resizeToHost() ultérieur déclenché par un
    // redimensionnement de l'hôte pendant que le menu est ouvert, voir
    // MenuResizeEventFilter plus haut) -- voir le commentaire sur
    // setResizeMode() ci-dessus pour pourquoi c'est nécessaire du tout.
    //
    // GetClientRect() est un appel Win32 pur : il retourne des pixels
    // PHYSIQUES. QQuickView::resize() attend des pixels INDÉPENDANTS DE LA
    // RÉSOLUTION (Qt 6 n'offre aucune bascule pour désactiver la mise à
    // l'échelle DPI). Sans diviser par devicePixelRatio(), la fenêtre de
    // menu et son panneau seraient mal dimensionnés/positionnés à toute
    // mise à l'échelle d'affichage Windows différente de 100% -- 125%/150%
    // sont les valeurs PAR DÉFAUT de Windows sur la plupart des portables et
    // écrans 4K. Trouvé en revue finale du projet ; non testé sur cette
    // machine, qui tourne à 100%, faute d'un second poste à échelle
    // différente disponible pour vérifier visuellement.
    auto syncMenuViewGeometryFromNativeWindow = [&menuView]() {
        RECT clientRect{};
        if (!GetClientRect(reinterpret_cast<HWND>(menuView.winId()), &clientRect)) return;
        const qreal dpr = menuView.devicePixelRatio();
        const int w = qRound((clientRect.right - clientRect.left) / dpr);
        const int h = qRound((clientRect.bottom - clientRect.top) / dpr);
        menuView.resize(w, h);
    };

    // toggleGameMenu() (pas openGameMenu()) : rappuyer sur Home pendant que
    // le menu est déjà ouvert doit reprendre le jeu -- demande explicite.
    QObject::connect(&inputManager, &InputManager::homeMenuRequested,
                      &emulatorProvider, &EmulatorProvider::toggleGameMenu);

    // Whole-branch review fix (use-after-free at app exit while the menu is
    // open): ces deux connect() capturent &gameMenuOverlay et &menuView par
    // référence dans leurs lambdas, sans objet de contexte -- sans lui,
    // aucun des deux QObject n'existant ici (ni emulatorProvider, ni les
    // lambdas elles-mêmes) ne fait automatiquement déconnecter ces
    // connexions à la destruction de gameMenuOverlay/menuView.
    // ~EmulatorProvider fait déjà, depuis le fix-wave précédent de cette
    // fonctionnalité, un kill()+waitForFinished() qui déclenche
    // synchroniquement QProcess::finished, lequel émet gameMenuClosed() si
    // le menu était ouvert -- et comme emulatorProvider est déclaré AVANT
    // gameMenuOverlay/menuView, il est détruit APRÈS eux (ordre inverse de
    // déclaration), donc ce chemin atteint la lambda alors que les deux
    // objets qu'elle capture sont déjà détruits. Trigger réel, pas un cas
    // limite théorique : mettre le jeu en pause via le menu, puis fermer la
    // fenêtre de Bili.
    //
    // Fix : passer un objet de contexte. gameMenuOverlay est déclaré AVANT
    // menuView, donc détruit APRÈS lui (ordre inverse) -- c'est donc lui qui
    // survit le plus longtemps des deux, et le passer en contexte garantit
    // que la connexion est coupée avant qu'AUCUN des deux ne devienne
    // invalide.
    QObject::connect(&emulatorProvider, &EmulatorProvider::gameMenuOpened, &gameMenuOverlay,
                      [&gameMenuOverlay, &menuView, rootWindow, &emulatorProvider,
                       syncMenuViewGeometryFromNativeWindow]() {
        // Whole-branch review fix (Fix 7): openGameMenu() a déjà posé
        // m_gameMenuOpen à true avant d'émettre ce signal -- si show()
        // échoue (ou si rootWindow est encore nul, cas extrême), il faut
        // annuler cet état via resumeGame() (renvoie PAUSE_TOGGLE + repose
        // le drapeau à false + émet gameMenuClosed()), sinon le jeu resterait
        // en pause sans aucun menu visible ni raison affichée, et (depuis le
        // Fix 5) bloquerait toute la navigation de Main.qml jusqu'à ce que
        // l'utilisateur trouve Cancel.
        if (!rootWindow) { emulatorProvider.resumeGame(); return; }
        if (!gameMenuOverlay.show(&menuView, rootWindow->winId(), emulatorProvider.gameWindowId())) {
            emulatorProvider.resumeGame();
            return;
        }
        syncMenuViewGeometryFromNativeWindow();
    });
    QObject::connect(&emulatorProvider, &EmulatorProvider::gameMenuClosed, &gameMenuOverlay,
                      [&gameMenuOverlay, &menuView, &emulatorProvider]() {
        gameMenuOverlay.hide(&menuView, emulatorProvider.gameWindowId());
    });

    // Task 3's report explicitly flags this as needed for Task 4: garde le
    // panneau/le voile bien dimensionnés et centrés si Bili est redimensionnée
    // pendant que le menu est ouvert (même resynchronisation manuelle que
    // ci-dessus, nécessaire pour la même raison).
    MenuResizeEventFilter menuResizeFilter(&emulatorProvider, &gameMenuOverlay,
                                            rootWindow ? rootWindow->winId() : WId(0),
                                            syncMenuViewGeometryFromNativeWindow);
    app.installNativeEventFilter(&menuResizeFilter);

    // Échap ouvre/ferme le menu en jeu et Entrée y déclenche "Quitter le
    // jeu", tous deux au clavier (voir MenuHotkeyEventFilter plus haut pour
    // pourquoi ceci passe par un hotkey système plutôt que par le clavier
    // QML habituel). Échap est enregistré/désenregistré sur tout le cycle
    // de vie d'une partie -- gameExited() est émis inconditionnellement
    // quel que soit le chemin de sortie (fin naturelle, quitGame(), crash),
    // donc cette seule paire de connexions couvre tous les cas sans
    // dupliquer le désenregistrement ailleurs. Entrée est enregistré/
    // désenregistré sur le cycle de vie du menu lui-même (gameMenuOpened/
    // gameMenuClosed), pas de la partie entière, pour qu'appuyer sur Entrée
    // pendant une partie SANS le menu ouvert n'ait aucun effet particulier
    // (comportement clavier normal dans ce cas).
    MenuHotkeyEventFilter menuHotkeyFilter(&emulatorProvider);
    app.installNativeEventFilter(&menuHotkeyFilter);
    if (rootWindow) {
        const HWND rootHwnd = reinterpret_cast<HWND>(rootWindow->winId());
        QObject::connect(&emulatorProvider, &EmulatorProvider::gameLaunched, [rootHwnd]() {
            RegisterHotKey(rootHwnd, kEscapeHotkeyId, MOD_NOREPEAT, VK_ESCAPE);
        });
        QObject::connect(&emulatorProvider, &EmulatorProvider::gameExited, [rootHwnd](int) {
            UnregisterHotKey(rootHwnd, kEscapeHotkeyId);
        });
        QObject::connect(&emulatorProvider, &EmulatorProvider::gameMenuOpened, [rootHwnd]() {
            RegisterHotKey(rootHwnd, kEnterHotkeyId, MOD_NOREPEAT, VK_RETURN);
        });
        QObject::connect(&emulatorProvider, &EmulatorProvider::gameMenuClosed, [rootHwnd]() {
            UnregisterHotKey(rootHwnd, kEnterHotkeyId);
        });
    }
#endif

    return app.exec();
}
