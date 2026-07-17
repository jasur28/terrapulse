#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "controllers/AppController.h"
#include "controllers/BusClient.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("TerraPulse");
    app.setOrganizationName("TerraPulse");

    // Pure viewer: raw stream from tpacq (5556) drives the live chart; analysis
    // results from tpproc (5557) drive the dashboards. No processing in the UI.
    BusClient     acq("tcp://127.0.0.1:5556", "raw.");
    AppController controller("tcp://127.0.0.1:5557");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appController", &controller);
    engine.rootContext()->setContextProperty("acq",           &acq);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("TerraPulse", "Main");
    return app.exec();
}
