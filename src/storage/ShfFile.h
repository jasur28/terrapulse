#pragma once
#include "core/ShfRecord.h"
#include <QString>
#include <vector>
#include <optional>

namespace tp {

class ShfFile {
public:
    static bool write(const QString& path, const ShfRecord& rec);
    static std::optional<ShfRecord> read(const QString& path);
    static bool writeAll(const QString& path, const std::vector<ShfRecord>& records);
    static std::vector<ShfRecord> readAll(const QString& path);
};

} // namespace tp
