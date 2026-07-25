// tpconfig — configuration editor (SeisComp scconfig). The first GUI an operator
// touches: define the structures and their sensors, tune each module's settings,
// and bind a structure to a profile — without hand-editing files or knowing the
// layer rules. It only writes to etc/ and the inventory JSON; nothing here talks
// to the live pipeline except the explicit "publish inventory" action.

#include "config/ConfigEditor.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <cstdio>
#include <cstring>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("tpconfig");
    app.setOrganizationName("TerraPulse");
    QQuickStyle::setStyle("Basic");     // our panels are custom-styled

    tp::ConfigEditor editor;

    // --selftest exercises the editor headlessly (no window): useful in CI and to
    // confirm an installation is writable before an operator relies on the GUI.
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--selftest") != 0) continue;
        std::printf("root:    %s\n", editor.root().toUtf8().constData());
        const QStringList mods = editor.modules();
        std::printf("modules: %s\n", mods.join(", ").toUtf8().constData());
        if (mods.contains("tpproc")) {
            std::printf("tpproc settings: %d keys\n", int(editor.settings("tpproc").size()));
            editor.setSetting("tpproc", "proc.staLta.onRatio", "4.25");
            std::printf("staged change -> hasChanges=%d\n", editor.hasChanges("tpproc") ? 1 : 0);
            editor.saveModule("tpproc");
            std::printf("%s\n", editor.status().toUtf8().constData());
        }
        editor.setBinding(9, 9, "tpproc", "highrise");
        std::printf("%s\n", editor.status().toUtf8().constData());
        std::printf("bindings: %d\n", int(editor.bindings().size()));
        editor.removeBinding(9, 9);
        const QVariantMap inv = editor.loadInventory();
        std::printf("inventory structures: %d\n",
                    int(inv.value("structures").toList().size()));
        return 0;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("editor", &editor);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("TPConfig", "Main");
    return app.exec();
}
