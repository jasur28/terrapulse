#pragma once
#include "core/SafRecord.h"
#include <QString>
#include <vector>
#include <optional>

namespace tp {

class SafFile {
public:
    static bool write(const QString& path, const SafRecord& rec);
    static std::optional<SafRecord> read(const QString& path);
    static bool writeAll(const QString& path, const std::vector<SafRecord>& records);
    static std::vector<SafRecord> readAll(const QString& path);
};

} // namespace tp
