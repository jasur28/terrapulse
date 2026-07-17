#include "storage/SafFile.h"
#include <QFile>
#include <QDataStream>

namespace tp {

static void writeRecord(QDataStream& ds, const SafRecord& r) {
    ds << r.formatVersion;
    ds << r.safId << r.sourceSdf;
    ds << r.stationId << r.objectId << r.sensorId;
    ds << static_cast<uint8_t>(r.component);
    ds << r.analysisStartTime << r.analysisEndTime;
    ds << r.sampleCount;
    ds << r.rms << r.variance << r.standardDeviation;
    ds << r.energy << r.maxAmplitude;
    ds << r.dominantFrequency << r.frequencyShift;
    ds << r.healthIndex;
    ds << static_cast<uint8_t>(r.anomalyDetected ? 1 : 0);
    ds << static_cast<uint8_t>(r.anomalyType);
    ds << static_cast<uint8_t>(r.warningLevel);
    ds << r.confidenceLevel;
    ds << r.processingTimeMs;
    ds << r.crc;
}

static bool readRecord(QDataStream& ds, SafRecord& r) {
    uint8_t comp, anom, warn, det;
    ds >> r.formatVersion;
    ds >> r.safId >> r.sourceSdf;
    ds >> r.stationId >> r.objectId >> r.sensorId;
    ds >> comp;
    r.component = static_cast<Axis>(comp);
    ds >> r.analysisStartTime >> r.analysisEndTime;
    ds >> r.sampleCount;
    ds >> r.rms >> r.variance >> r.standardDeviation;
    ds >> r.energy >> r.maxAmplitude;
    ds >> r.dominantFrequency >> r.frequencyShift;
    ds >> r.healthIndex;
    ds >> det >> anom >> warn;
    r.anomalyDetected = (det != 0);
    r.anomalyType     = static_cast<AnomalyType>(anom);
    r.warningLevel    = static_cast<WarningLevel>(warn);
    ds >> r.confidenceLevel;
    ds >> r.processingTimeMs;
    ds >> r.crc;
    return ds.status() == QDataStream::Ok;
}

bool SafFile::write(const QString& path, const SafRecord& rec) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    writeRecord(ds, rec);
    return ds.status() == QDataStream::Ok;
}

std::optional<SafRecord> SafFile::read(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return std::nullopt;
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    SafRecord r;
    if (!readRecord(ds, r)) return std::nullopt;
    return r;
}

bool SafFile::writeAll(const QString& path, const std::vector<SafRecord>& records) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    ds << static_cast<uint32_t>(records.size());
    for (const auto& r : records) writeRecord(ds, r);
    return ds.status() == QDataStream::Ok;
}

std::vector<SafRecord> SafFile::readAll(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QDataStream ds(&f);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);
    uint32_t count = 0;
    ds >> count;
    std::vector<SafRecord> result;
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        SafRecord r;
        if (!readRecord(ds, r)) break;
        result.push_back(std::move(r));
    }
    return result;
}

} // namespace tp
