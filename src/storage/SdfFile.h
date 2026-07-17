#pragma once
#include "core/SdfRecord.h"
#include <QString>
#include <vector>
#include <optional>

namespace tp {

class SdfFile {
public:
    static bool write(const QString& path, const SdfRecord& rec);
    static std::optional<SdfRecord> read(const QString& path);

    // Append multiple records to one file (sequential format).
    static bool writeAll(const QString& path, const std::vector<SdfRecord>& records);
    static std::vector<SdfRecord> readAll(const QString& path);
};

} // namespace tp
