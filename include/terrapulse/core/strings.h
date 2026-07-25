#pragma once

#include <QString>
#include <QStringList>

namespace tp::core::strings {

inline QString trim(const QString& value) {
    return value.trimmed();
}

inline QStringList split(const QString& value, QChar separator) {
    return value.split(separator, Qt::SkipEmptyParts);
}

} // namespace tp::core::strings
