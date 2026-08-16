#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>

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
#include "emulators/StubEmulatorProvider.h"
#include "emulators/EmulatorProviderQmlBridge.h"
#include "scraper/StubScraperProvider.h"
#include "scraper/ScraperProviderQmlBridge.h"
#include "netplay/StubNetplaySession.h"
#include "netplay/NetplaySessionQmlBridge.h"

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

    StubEmulatorProvider emulatorProvider;
    EmulatorProviderQmlBridge emulatorBridge(&emulatorProvider);
    engine.rootContext()->setContextProperty("EmulatorProvider", &emulatorBridge);

    StubScraperProvider scraperProvider;
    ScraperProviderQmlBridge scraperBridge(&scraperProvider);
    engine.rootContext()->setContextProperty("ScraperProvider", &scraperBridge);

    StubNetplaySession netplaySession;
    NetplaySessionQmlBridge netplayBridge(&netplaySession);
    engine.rootContext()->setContextProperty("NetplaySession", &netplayBridge);

    SystemController systemController;
    engine.rootContext()->setContextProperty("SystemController", &systemController);

    engine.loadFromModule("Bili", "Main");

    return app.exec();
}
