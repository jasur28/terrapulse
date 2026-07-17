#include "storage/ShfFile.h"
#include <QFile>
#include <QDataStream>

namespace tp {

static void writeRecord(QDataStream& ds, const ShfRecord& r) {
    ds << r.formatVersion;
    ds << r.shfId << r.sourceSaf;
    ds << r.objectId << r.sensorId;
    ds << static_cast<uint8_t>(r.component);
    ds << static_cast<uint8_t>(r.anomalyType);
    ds << r.anomalyStartTime << r.anomalyEndTime;
    ds << static_cast<uint8_t>(r.anomalyStatus);
    ds << r.anomalyDuration;
    ds << r.maxValue << r.averageValue << r.growthRate;
    ds << static_cast<uint8_t>(r.severityLevel);
    ds << r.confidenceLevel;
    ds << static_cast<uint8_t>(r.trend);
    ds << r.sampleCount;
    ds << static_cast<uint32_t>(r.timeline.size());
    for (const auto& ref : r.timeline)
        ds << ref.safId << ref.timestamp;
    QByteArray notesBytes = QByteArray::fromStdString(r.notes);
    ds << static_cast<uint32_t>(notesBytes.size());
    if (!notesBytes.isEmpty())
        ds.writeRawData(notesBytes.constData(), notesBytes.size());
    ds << r.crc;
}

static bool readRecord(QDataStream& ds, ShfRecord& r) {
    uint8_t comp, atype, astatus, sev, trend;
    ds >> r.formatVersion;
    ds >> r.shfId >> r.sourceSaf;
    ds >> r.objectId >> r.sensorId;
    ds >> comp >> atype;
    r.component   = static_cast<Axis>(comp);
    r.anomalyType = static_cast<AnomalyType>(atype);
    ds >> r.anomalyStartTime >> r.anomalyEndTime;
    ds >> astatus;
    r.anomalyStatus = static_cast<AnomalyStatus>(astatus);
    ds >> r.anomalyDuration;
    ds >> r.maxValue >> r.averageValue >> r.growthRate;
    ds >> sev >> r.confidenceLevel >> trend;
    r.severityLevel = static_cast<SeverityLevel>(sev);
    r.trend         = static_cast<Trend>(trend);
    ds >> r.sampleCount;
    uint32_t timelineSize = 0;
    ds >> timelineSize;
    r.timeline.resize(timelineSize);
    for (auto& ref : r.timeline)
        ds >> ref.safId >> ref.timestamp;
    uint32_t notesLen = 0;
    ds >> notesLen;
    if (notesLen > 0) {
        QByteArray buf(static_cast<int>(notesLen), '\0');
        ds.readRawData(buf.data(), static_cast<int>(notesLen));
        r.notes = buf.toStdString();
    }
    ds >> r.crc;
    return ds.status() == QDataStream::Ok;
}

bool ShfFile::write(const QString& path, const ShfRecord& rec) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    writeRecord(ds, rec);
    return ds.status() == QDataStream::Ok;
}

std::optional<ShfRecord> ShfFile::read(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return std::nullopt;
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    ShfRecord r;
    if (!readRecord(ds, r)) return std::nullopt;
    return r;
}

bool ShfFile::writeAll(const QString& path, const std::vector<ShfRecord>& records) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    ds << static_cast<uint32_t>(records.size());
    for (const auto& r : records) writeRecord(ds, r);
    return ds.status() == QDataStream::Ok;
}

std::vector<ShfRecord> ShfFile::readAll(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    uint32_t count = 0;
    ds >> count;
    std::vector<ShfRecord> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        ShfRecord r;
        if (!readRecord(ds, r)) break;
        result.push_back(std::move(r));
    }
    return result;
}

} // namespace tp
