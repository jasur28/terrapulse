// tpdumpcfg — print the effective configuration for a module and show which
// files contributed (SeisComp's scdumpcfg). Answers "which value is actually in
// force, and where do I change it?" without starting the module.
//
//   tpdumpcfg tpproc            effective config for tpproc
//   tpdumpcfg tpproc --files    also list the layers that were read
//   tpdumpcfg --list            modules that ship defaults

#include "config/Config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <cstdio>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    QString module;
    bool showFiles = false, list = false;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == "--files")      showFiles = true;
        else if (a == "--list")  list = true;
        else if (!a.startsWith("-")) module = a;
    }

    const QString root = tp::Config::discoverRoot();
    if (root.isEmpty()) {
        std::fprintf(stderr, "tpdumpcfg: no installation root found "
                             "(set TP_ROOT or run where etc/ exists)\n");
        return 1;
    }

    if (list || module.isEmpty()) {
        std::printf("root: %s\n\nmodules with shipped defaults:\n", root.toUtf8().constData());
        QDir d(root + "/etc/defaults");
        for (const QFileInfo& fi : d.entryInfoList({"*.cfg"}, QDir::Files, QDir::Name))
            std::printf("  %s\n", fi.completeBaseName().toUtf8().constData());
        if (module.isEmpty()) {
            std::printf("\nusage: tpdumpcfg <module> [--files]\n");
            return 0;
        }
    }

    tp::Config cfg;
    cfg.load(module, root);

    std::printf("root:   %s\nmodule: %s\n\n", root.toUtf8().constData(),
                module.toUtf8().constData());
    if (showFiles) {
        std::printf("layers read (later overrides earlier):\n");
        for (const QString& f : cfg.loadedFiles())
            std::printf("  %s\n", f.toUtf8().constData());
        std::printf("\n");
    }

    std::printf("effective configuration (%d keys):\n", int(cfg.all().size()));
    for (const auto& kv : cfg.all())
        std::printf("  %-34s = %s\n", kv.first.toUtf8().constData(),
                    kv.second.toUtf8().constData());
    return 0;
}
