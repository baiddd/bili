#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>
#include <QWindow>

#ifdef Q_OS_WIN
#include <QAbstractNativeEventFilter>
#include <windows.h>
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

    return app.exec();
}
