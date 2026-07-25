#include "mseed/RecordStream.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

namespace fs = std::filesystem;

namespace tp::mseed {

namespace {

std::string sanitize(std::string s) {
    for (char& c : s) if (c == ':') c = '_';   // Windows-safe (matches TdsArchive)
    return s;
}

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
}

// Concatenate time-ordered records of ONE source id into a single Trace,
// counting (but not filling) timing gaps between records.
Trace merge(std::vector<Decoded>&& recs) {
    Trace t;
    if (recs.empty()) return t;
    std::sort(recs.begin(), recs.end(),
              [](const Decoded& a, const Decoded& b) { return a.startTimeMs < b.startTimeMs; });

    t.sid         = recs.front().sid;
    t.sampleRate  = recs.front().sampleRate;
    t.startTimeMs = recs.front().startTimeMs;

    int64_t nextExpectedMs = t.startTimeMs;
    const double dtMs = t.sampleRate > 0 ? 1000.0 / t.sampleRate : 0.0;
    for (auto& r : recs) {
        if (dtMs > 0 && std::llabs(r.startTimeMs - nextExpectedMs) > dtMs / 2)
            ++t.gaps;
        t.samples.insert(t.samples.end(), r.samples.begin(), r.samples.end());
        nextExpectedMs = r.startTimeMs + int64_t(std::llround(r.samples.size() * dtMs));
    }
    return t;
}

// Group a bag of decoded records by source id and merge each group.
std::vector<Trace> groupBySid(std::vector<Decoded>&& recs) {
    std::map<std::string, std::vector<Decoded>> bySid;
    for (auto& r : recs) bySid[r.sid].push_back(std::move(r));
    std::vector<Trace> out;
    out.reserve(bySid.size());
    for (auto& [sid, group] : bySid) out.push_back(merge(std::move(group)));
    return out;
}

// Strip a known scheme prefix if present.
bool strip(std::string& s, const char* scheme) {
    const std::string p = scheme;
    if (s.rfind(p, 0) == 0) { s.erase(0, p.size()); return true; }
    return false;
}

} // namespace

Trace readTdsChannel(const std::string& root, const std::string& sid) {
    const std::string prefix = sanitize(sid) + ".";
    std::vector<std::string> files;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(root, ec))
        if (e.path().filename().string().rfind(prefix, 0) == 0)
            files.push_back(e.path().string());
    std::sort(files.begin(), files.end());   // day files sort chronologically

    std::vector<Decoded> recs;
    for (const auto& f : files) {
        const std::string buf = slurp(f);
        auto d = decode(buf.data(), buf.size());
        recs.insert(recs.end(), std::make_move_iterator(d.begin()),
                                std::make_move_iterator(d.end()));
    }
    return merge(std::move(recs));
}

std::vector<Trace> readFile(const std::string& path) {
    const std::string buf = slurp(path);
    return groupBySid(decode(buf.data(), buf.size()));
}

std::vector<Trace> readCsv(const std::string& path, uint32_t object, uint32_t sensor,
                           double sampleRate) {
    // Collect the files to read: one file, or every file in a directory.
    std::vector<std::string> files;
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        for (const auto& e : fs::directory_iterator(path, ec))
            if (e.is_regular_file()) files.push_back(e.path().string());
        std::sort(files.begin(), files.end());
    } else {
        files.push_back(path);
    }

    Trace tx, ty, tz;
    bool first = true;
    for (const std::string& f : files) {
        std::ifstream in(f);
        if (!in) continue;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#' || line[0] == 't') continue;   // skip header
            // t_ms,x,y,z  (values in gal)
            double v[4] = {0, 0, 0, 0};
            int n = 0;
            std::size_t pos = 0;
            while (n < 4 && pos <= line.size()) {
                const std::size_t comma = line.find(',', pos);
                const std::string tok = line.substr(pos, comma == std::string::npos
                                                        ? std::string::npos : comma - pos);
                try { v[n++] = std::stod(tok); } catch (...) { n = 0; break; }
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
            if (n < 4) continue;
            if (first) {
                tx.startTimeMs = ty.startTimeMs = tz.startTimeMs = int64_t(v[0]);
                first = false;
            }
            // Same counts convention as the archive: gal x 10000 -> int32.
            tx.samples.push_back(int32_t(std::lround(v[1] * 10000.0)));
            ty.samples.push_back(int32_t(std::lround(v[2] * 10000.0)));
            tz.samples.push_back(int32_t(std::lround(v[3] * 10000.0)));
        }
    }
    if (first) return {};        // nothing parsed

    const double rate = sampleRate > 0 ? sampleRate : 200.0;
    tx.sampleRate = ty.sampleRate = tz.sampleRate = rate;
    tx.sid = sourceId(object, sensor, 0);
    ty.sid = sourceId(object, sensor, 1);
    tz.sid = sourceId(object, sensor, 2);
    return { std::move(tx), std::move(ty), std::move(tz) };
}

std::vector<Trace> readStream(const std::string& uri) {
    std::string u = uri;

    if (strip(u, "file://")) return readFile(u);

    if (strip(u, "combined://")) {
        // combined://(uriA,uriB,...) — merge several sources, e.g. the on-disk
        // archive plus data recovered from a sensor's SD card.
        if (!u.empty() && u.front() == '(') u.erase(0, 1);
        if (!u.empty() && u.back()  == ')') u.pop_back();
        std::vector<Trace> out;
        std::size_t i = 0;
        int depth = 0;
        std::string part;
        for (; i <= u.size(); ++i) {
            const char c = i < u.size() ? u[i] : ',';
            if (c == '(') ++depth;
            if (c == ')') --depth;
            if (c == ',' && depth == 0) {
                if (!part.empty()) {
                    auto sub = readStream(part);
                    out.insert(out.end(), std::make_move_iterator(sub.begin()),
                                          std::make_move_iterator(sub.end()));
                    part.clear();
                }
            } else {
                part.push_back(c);
            }
        }
        return out;
    }

    if (strip(u, "csv://")) {
        // csv://<path>[?obj=..&sen=..&rate=..]
        std::string p = u, query;
        if (auto q = u.find('?'); q != std::string::npos) {
            p     = u.substr(0, q);
            query = u.substr(q + 1);
        }
        long obj = 1, sen = 1; double rate = 200.0;
        for (std::size_t i = 0; i < query.size();) {
            auto amp = query.find('&', i);
            std::string kv = query.substr(i, amp == std::string::npos ? std::string::npos : amp - i);
            if (auto eq = kv.find('='); eq != std::string::npos) {
                const std::string k = kv.substr(0, eq), v = kv.substr(eq + 1);
                try {
                    if      (k == "obj")  obj  = std::stol(v);
                    else if (k == "sen")  sen  = std::stol(v);
                    else if (k == "rate") rate = std::stod(v);
                } catch (...) {}
            }
            if (amp == std::string::npos) break;
            i = amp + 1;
        }
        return readCsv(p, uint32_t(obj), uint32_t(sen), rate);
    }

    if (strip(u, "tds://")) {
        // tds://<root>[?obj=..&sen=..]
        std::string root = u, query;
        if (auto q = u.find('?'); q != std::string::npos) {
            root  = u.substr(0, q);
            query = u.substr(q + 1);
        }
        long obj = -1, sen = -1;
        for (size_t i = 0; i < query.size();) {
            auto amp = query.find('&', i);
            std::string kv = query.substr(i, amp == std::string::npos ? std::string::npos : amp - i);
            if (auto eq = kv.find('='); eq != std::string::npos) {
                const std::string k = kv.substr(0, eq), v = kv.substr(eq + 1);
                if (k == "obj") obj = std::stol(v);
                else if (k == "sen") sen = std::stol(v);
            }
            if (amp == std::string::npos) break;
            i = amp + 1;
        }
        if (obj >= 0 && sen >= 0) {                 // one sensor: X/Y/Z
            std::vector<Trace> out;
            for (int comp = 0; comp < 3; ++comp) {
                Trace t = readTdsChannel(root, sourceId(uint32_t(obj), uint32_t(sen), comp));
                if (!t.samples.empty()) out.push_back(std::move(t));
            }
            return out;
        }
        // whole archive: decode every .mseed file, group by sid
        std::vector<Decoded> recs;
        std::error_code ec;
        for (auto& e : fs::directory_iterator(root, ec)) {
            if (e.path().extension() != ".mseed") continue;
            const std::string buf = slurp(e.path().string());
            auto d = decode(buf.data(), buf.size());
            recs.insert(recs.end(), std::make_move_iterator(d.begin()),
                                    std::make_move_iterator(d.end()));
        }
        return groupBySid(std::move(recs));
    }

    // Bare path: directory -> TDS archive, file -> single miniSEED.
    std::error_code ec;
    if (fs::is_directory(u, ec)) return readStream("tds://" + u);
    return readFile(u);
}

} // namespace tp::mseed
