#pragma once

#include <QByteArray>
#include <QString>

namespace tp::core::io {

QByteArray readAll(const QString& path);
bool writeAll(const QString& path, const QByteArray& data);

} // namespace tp::core::io
