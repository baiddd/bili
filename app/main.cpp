#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "input/InputManager.h"
#include "ui/ScreenManager.h"

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

    engine.loadFromModule("Bili", "Main");

    return app.exec();
}
