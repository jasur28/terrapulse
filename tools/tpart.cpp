// tpart — archive tool (SeisComp scart). Checks, exports and imports the miniSEED
// TDS archive. `check` is the important one: it verifies that what we recorded is
// actually continuous, because a monitoring archive with silent holes is worse
// than no archive.
//
//   tpart check  <uri>                     continuity/gap report per channel
//   tpart export <uri> --out file.mseed    copy matching records into one file
//   tpart import <file.mseed> --tds <dir>  fold an external file into the archive

#include "mseed/RecordStream.h"
#include "mseed/Mseed.h"
#include "mseed/TdsArchive.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::string isoUtc(int64_t ms) {
    std::time_t s = std::time_t(ms / 1000);
    std::tm tmv{};
#if defined(_WIN32)
    gmtime_s(&tmv, &s);
#else
    gmtime_r(&s, &tmv);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmv);
    return buf;
}

const char* argAfter(int argc, char** argv, const char* key) {
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], key) == 0) return argv[i + 1];
    return nullptr;
}

int cmdCheck(const std::string& uri) {
    auto traces = tp::mseed::readStream(uri);
    if (traces.empty()) {
        std::fprintf(stderr, "tpart: nothing found at %s\n", uri.c_str());
        return 1;
    }
    int problems = 0;
    std::printf("%-22s %-21s %10s %6s %6s %s\n",
                "source-id", "start (UTC)", "samples", "Hz", "gaps", "verdict");
    for (const auto& t : traces) {
        const double durS = t.sampleRate > 0 ? double(t.samples.size()) / t.sampleRate : 0.0;
        const char* verdict = t.gaps == 0 ? "CONTINUOUS" : "HAS GAPS";
        if (t.gaps != 0) ++problems;
        std::printf("%-22s %-21s %10zu %6.0f %6d %s (%.1f s)\n",
                    t.sid.c_str(), isoUtc(t.startTimeMs).c_str(), t.samples.size(),
                    t.sampleRate, t.gaps, verdict, durS);
    }
    std::printf("\n%d channel(s), %d with gaps\n", int(traces.size()), problems);
    return problems == 0 ? 0 : 1;      // non-zero so a scheduled check can alert
}

int cmdExport(const std::string& uri, const char* outPath) {
    if (!outPath) { std::fprintf(stderr, "tpart export: --out <file> required\n"); return 2; }
    auto traces = tp::mseed::readStream(uri);
    if (traces.empty()) { std::fprintf(stderr, "tpart: nothing found at %s\n", uri.c_str()); return 1; }

    std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
    if (!out) { std::fprintf(stderr, "tpart: cannot write %s\n", outPath); return 1; }

    std::size_t records = 0, samples = 0;
    for (const auto& t : traces) {
        if (t.samples.empty() || t.sampleRate <= 0) continue;
        tp::mseed::encode(t.sid, t.sampleRate, t.startTimeMs, t.samples,
                          [&](const char* rec, int len) {
                              out.write(rec, len);
                              ++records;
                          });
        samples += t.samples.size();
    }
    out.close();
    std::printf("tpart: wrote %zu record(s), %zu sample(s) from %zu channel(s) to %s\n",
                records, samples, traces.size(), outPath);
    return 0;
}

int cmdImport(const std::string& file, const char* tdsDir) {
    if (!tdsDir) { std::fprintf(stderr, "tpart import: --tds <dir> required\n"); return 2; }
    auto traces = tp::mseed::readStream("file://" + file);
    if (traces.empty()) { std::fprintf(stderr, "tpart: no records in %s\n", file.c_str()); return 1; }

    // Fold each trace back into the archive under its own source id. Object and
    // sensor are parsed from our FDSN naming (TP_<obj>_<sen>_H_N_<axis>).
    tp::mseed::TdsArchive tds(tdsDir);
    std::size_t written = 0;
    for (const auto& t : traces) {
        unsigned obj = 0, sen = 0; char axis = 'Z';
        if (std::sscanf(t.sid.c_str(), "FDSN:TP_%u_%u_H_N_%c", &obj, &sen, &axis) != 3) {
            std::printf("  ! skipping unrecognised source id %s\n", t.sid.c_str());
            continue;
        }
        const int comp = axis == 'X' ? 0 : axis == 'Y' ? 1 : 2;
        const double dtMs = 1000.0 / t.sampleRate;
        for (std::size_t i = 0; i < t.samples.size(); ++i)
            tds.addSample(obj, sen, comp, t.samples[i],
                          t.startTimeMs + int64_t(i * dtMs), t.sampleRate);
        written += t.samples.size();
    }
    tds.flushAll();
    std::printf("tpart: imported %zu sample(s) into %s\n", written, tdsDir);
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::fprintf(stderr,
            "usage:\n"
            "  tpart check  <uri>\n"
            "  tpart export <uri> --out <file.mseed>\n"
            "  tpart import <file.mseed> --tds <dir>\n");
        return 2;
    }
    const std::string cmd = argv[1];
    const std::string target = argv[2];

    if (cmd == "check")  return cmdCheck(target);
    if (cmd == "export") return cmdExport(target, argAfter(argc, argv, "--out"));
    if (cmd == "import") return cmdImport(target, argAfter(argc, argv, "--tds"));

    std::fprintf(stderr, "tpart: unknown command '%s'\n", cmd.c_str());
    return 2;
}
