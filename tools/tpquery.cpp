// tpquery — run named, parameterised queries from a config file (SeisComp scquery).
// Operators get useful reports without writing SQL or being given write access;
// the query set lives in etc/queries.cfg and is reviewed like any other config.
//
//   tpquery --list
//   tpquery worst-health 5
//   tpquery events-since 2026-07-01
//
// etc/queries.cfg format:  <name> = <SQL with ? placeholders>   # description

#include "config/Config.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <cstdio>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("tpquery");

    tp::Config cfg;
    cfg.load("tpquery");

    QCommandLineParser parser;
    parser.setApplicationDescription("Run a named query from etc/queries.cfg");
    parser.addHelpOption();
    QCommandLineOption dbOpt("db", "SQLite database", "path",
                             cfg.str("database.path", "terrapulse.db"));
    QCommandLineOption listOpt("list", "List the available queries");
    QCommandLineOption fileOpt("queries", "Query definition file", "path");
    parser.addOptions({dbOpt, listOpt, fileOpt});
    parser.addPositionalArgument("name", "Query name");
    parser.addPositionalArgument("args", "Query arguments", "[args...]");
    parser.process(app);

    // Query definitions live next to the other configuration.
    QString qfile = parser.value(fileOpt);
    if (qfile.isEmpty()) {
        const QString root = tp::Config::discoverRoot();
        qfile = root.isEmpty() ? QString("etc/queries.cfg") : root + "/etc/queries.cfg";
    }

    QMap<QString, QString> queries, descriptions;
    QFile f(qfile);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith('#')) continue;
            const int eq = line.indexOf('=');
            if (eq <= 0) continue;
            const QString name = line.left(eq).trimmed();
            QString sql = line.mid(eq + 1).trimmed();
            const int hash = sql.lastIndexOf('#');
            if (hash > 0) { descriptions[name] = sql.mid(hash + 1).trimmed(); sql = sql.left(hash).trimmed(); }
            queries[name] = sql;
        }
    }

    const QStringList pos = parser.positionalArguments();
    if (parser.isSet(listOpt) || pos.isEmpty()) {
        std::printf("queries from %s:\n", qfile.toUtf8().constData());
        if (queries.isEmpty()) std::printf("  (none — create the file)\n");
        for (auto it = queries.begin(); it != queries.end(); ++it)
            std::printf("  %-20s %s\n", it.key().toUtf8().constData(),
                        descriptions.value(it.key()).toUtf8().constData());
        return pos.isEmpty() && !parser.isSet(listOpt) ? 2 : 0;
    }

    const QString name = pos.first();
    if (!queries.contains(name)) {
        std::fprintf(stderr, "tpquery: unknown query '%s' (try --list)\n", name.toUtf8().constData());
        return 2;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "tpquery");
    db.setDatabaseName(parser.value(dbOpt));
    db.setConnectOptions("QSQLITE_OPEN_READONLY");
    if (!db.open()) {
        std::fprintf(stderr, "tpquery: cannot open '%s': %s\n",
                     parser.value(dbOpt).toUtf8().constData(),
                     db.lastError().text().toUtf8().constData());
        return 1;
    }

    QSqlQuery q(db);
    q.prepare(queries[name]);
    for (int i = 1; i < pos.size(); ++i) q.bindValue(i - 1, pos[i]);
    if (!q.exec()) {
        std::fprintf(stderr, "tpquery: %s\n", q.lastError().text().toUtf8().constData());
        return 1;
    }

    bool header = false;
    int rows = 0;
    while (q.next()) {
        const QSqlRecord r = q.record();
        if (!header) {
            for (int i = 0; i < r.count(); ++i)
                std::printf("%-22s", r.fieldName(i).toUtf8().constData());
            std::printf("\n");
            header = true;
        }
        for (int i = 0; i < r.count(); ++i)
            std::printf("%-22s", r.value(i).toString().toUtf8().constData());
        std::printf("\n");
        ++rows;
    }
    std::printf("(%d row%s)\n", rows, rows == 1 ? "" : "s");
    return 0;
}
