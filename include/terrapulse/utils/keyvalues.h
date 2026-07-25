#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace tp::utils {

inline QVariantMap parseKeyValues(const QStringList& lines, QChar separator = '=') {
    QVariantMap out;
    for (QString line : lines) {
        const int hash = line.indexOf('#');
        if (hash >= 0) {
            line = line.left(hash);
        }
        line = line.trimmed();
        const int pos = line.indexOf(separator);
        if (pos <= 0) {
            continue;
        }
        out.insert(line.left(pos).trimmed(), line.mid(pos + 1).trimmed());
    }
    return out;
}

inline QStringList writeKeyValues(const QVariantMap& values, QChar separator = '=') {
    QStringList out;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        out << QStringLiteral("%1 %2 %3").arg(it.key(), QString(separator), it.value().toString());
    }
    return out;
}

} // namespace tp::utils
