#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

namespace tp::io {

class Database {
public:
    bool openSQLite(const QString& path, const QString& connectionName = {});
    void close();
    bool isOpen() const;
    QSqlQuery exec(const QString& sql);
    QSqlDatabase handle() const { return m_db; }

private:
    QSqlDatabase m_db;
    QString m_connectionName;
};

} // namespace tp::io
