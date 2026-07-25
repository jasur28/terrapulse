#include "mseed/TdsArchive.h"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace tp::mseed {

TdsArchive::TdsArchive(std::string root, int recordSamples, int encoding)
    : m_root(std::move(root)), m_recSamples(recordSamples < 8 ? 8 : recordSamples), m_encoding(encoding) {
    std::error_code ec;
    fs::create_directories(m_root, ec);
}

TdsArchive::~TdsArchive() { flushAll(); }

namespace {
uint64_t chanKey(uint32_t objectId, uint32_t sensorId, int component) {
    return (static_cast<uint64_t>(objectId) << 20)
         | (static_cast<uint64_t>(sensorId) << 4)
         | static_cast<uint64_t>(component & 0xF);
}
} // namespace

void TdsArchive::setSourceId(uint32_t objectId, uint32_t sensorId, int component,
                             std::string sid) {
    m_chans[chanKey(objectId, sensorId, component)].sid = std::move(sid);
}

void TdsArchive::addSample(uint32_t objectId, uint32_t sensorId, int component,
                           int32_t value, int64_t timeMs, double sampleRate) {
    Chan& c = m_chans[chanKey(objectId, sensorId, component)];
    if (c.buf.empty()) {
        // Bound by the caller (setSourceId) -> use it; otherwise fall back to
        // the legacy numeric id so unbound callers behave exactly as before.
        if (c.sid.empty()) c.sid = sourceId(objectId, sensorId, component);
        c.sampleRate = sampleRate;
        c.startMs    = timeMs;
    }
    c.buf.push_back(value);
    if (static_cast<int>(c.buf.size()) >= m_recSamples)
        flush(c);
}

void TdsArchive::flush(Chan& c) {
    if (c.buf.empty()) return;

    const std::string path = filePath(c.sid, c.startMs);
    std::ofstream out(path, std::ios::binary | std::ios::app);
    if (out) {
        encode(c.sid, c.sampleRate, c.startMs, c.buf,
               [&](const char* rec, int len) { out.write(rec, len); ++m_records; },
               m_encoding);
    }
    c.buf.clear();
}

void TdsArchive::flushAll() {
    for (auto& kv : m_chans) flush(kv.second);
}

std::string TdsArchive::filePath(const std::string& sid, int64_t timeMs) const {
    std::time_t t = static_cast<std::time_t>(timeMs / 1000);
    std::tm g{};
#if defined(_WIN32)
    gmtime_s(&g, &t);
#else
    gmtime_r(&t, &g);
#endif
    // ':' is illegal in Windows filenames — sanitise the source id.
    std::string safe = sid;
    for (char& ch : safe) if (ch == ':') ch = '_';

    char name[128];
    std::snprintf(name, sizeof name, "%s.%04d.%03d.mseed", safe.c_str(), g.tm_year + 1900, g.tm_yday + 1);
    return (fs::path(m_root) / name).string();
}

} // namespace tp::mseed
