#include "storage/SdfFile.h"
#include <QFile>
#include <QDataStream>

namespace tp {

// Binary layout per record:
//   [uint8]  formatVersion
//   [uint32] stationId, objectId, sensorId
//   [uint8]  component, unit, sensorStatus
//   [uint32] samplingRate, sampleCount
//   [int64]  startTime, endTime
//   [float]  temperature
//   [uint8]  batteryLevel
//   [uint32] crc
//   [float*] waveformData (sampleCount floats)

static void writeRecord(QDataStream& ds, const SdfRecord& r) {
    ds << r.formatVersion;
    ds << r.stationId << r.objectId << r.sensorId;
    ds << static_cast<uint8_t>(r.component);
    ds << static_cast<uint8_t>(r.unit);
    ds << static_cast<uint8_t>(r.sensorStatus);
    ds << r.samplingRate << r.sampleCount;
    ds << r.startTime << r.endTime;
    ds << r.temperature;
    ds << r.batteryLevel;
    ds << r.crc;
    for (float v : r.waveformData)
        ds << v;
}

static bool readRecord(QDataStream& ds, SdfRecord& r) {
    uint8_t comp, unit, status;
    ds >> r.formatVersion;
    ds >> r.stationId >> r.objectId >> r.sensorId;
    ds >> comp >> unit >> status;
    r.component    = static_cast<Axis>(comp);
    r.unit         = static_cast<MeasurementUnit>(unit);
    r.sensorStatus = static_cast<SensorStatus>(status);
    ds >> r.samplingRate >> r.sampleCount;
    ds >> r.startTime >> r.endTime;
    ds >> r.temperature;
    ds >> r.batteryLevel;
    ds >> r.crc;
    r.waveformData.resize(r.sampleCount);
    for (uint32_t i = 0; i < r.sampleCount; ++i)
        ds >> r.waveformData[i];
    return ds.status() == QDataStream::Ok;
}

bool SdfFile::write(const QString& path, const SdfRecord& rec) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    writeRecord(ds, rec);
    return ds.status() == QDataStream::Ok;
}

std::optional<SdfRecord> SdfFile::read(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return std::nullopt;
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    SdfRecord r;
    if (!readRecord(ds, r)) return std::nullopt;
    return r;
}

bool SdfFile::writeAll(const QString& path, const std::vector<SdfRecord>& records) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    ds << static_cast<uint32_t>(records.size());
    for (const auto& r : records)
        writeRecord(ds, r);
    return ds.status() == QDataStream::Ok;
}

std::vector<SdfRecord> SdfFile::readAll(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    uint32_t count = 0;
    ds >> count;
    std::vector<SdfRecord> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        SdfRecord r;
        if (!readRecord(ds, r)) break;
        result.push_back(std::move(r));
    }
    return result;
}

} // namespace tp
