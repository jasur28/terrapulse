// mseed_smoke — verify the tp::mseed wrapper: build a source id, encode a window
// of samples into miniSEED, decode it back, and check the roundtrip.

#include "mseed/Mseed.h"

#include <cstdio>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    // File mode: decode a miniSEED file (e.g. a TDS archive file) and summarise.
    if (argc > 1) {
        std::ifstream f(argv[1], std::ios::binary);
        std::string buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto recs = tp::mseed::decode(buf.data(), buf.size());
        long long total = 0;
        for (auto& d : recs) total += (long long)d.samples.size();
        std::printf("file: %s  bytes=%zu  records=%zu  samples=%lld\n",
                    argv[1], buf.size(), recs.size(), total);
        if (!recs.empty())
            std::printf("first: sid=%s sr=%.1f startMs=%lld  first3=%d,%d,%d\n",
                        recs[0].sid.c_str(), recs[0].sampleRate, (long long)recs[0].startTimeMs,
                        recs[0].samples.size() > 0 ? recs[0].samples[0] : 0,
                        recs[0].samples.size() > 1 ? recs[0].samples[1] : 0,
                        recs[0].samples.size() > 2 ? recs[0].samples[2] : 0);
        return recs.empty() ? 1 : 0;
    }

    const std::string sid = tp::mseed::sourceId(1, 1, 2);   // object 1, sensor 1, Z
    std::printf("source id: %s\n", sid.c_str());

    const int N = 200;
    std::vector<int32_t> samples(N);
    for (int i = 0; i < N; ++i)
        samples[i] = static_cast<int32_t>(std::lround(1000.0 * std::sin(2.0 * 3.14159265358979 * 3.0 * i / 200.0)));

    // encode
    std::string packed;
    bool ok = tp::mseed::encode(sid, 200.0, 1704067200000LL, samples,
                                [&](const char* rec, int len) { packed.append(rec, len); });
    std::printf("encoded: ok=%d bytes=%zu\n", ok ? 1 : 0, packed.size());

    // decode
    auto recs = tp::mseed::decode(packed.data(), packed.size());
    if (recs.empty()) { std::printf("decode produced no records\n"); return 1; }

    const auto& d = recs.front();
    int mism = 0;
    for (int i = 0; i < static_cast<int>(d.samples.size()) && i < N; ++i)
        if (d.samples[i] != samples[i]) ++mism;

    std::printf("decoded: sid=%s sr=%.1f startMs=%lld samples=%zu mismatches=%d\n",
                d.sid.c_str(), d.sampleRate, static_cast<long long>(d.startTimeMs),
                d.samples.size(), mism);

    const bool good = ok && recs.size() == 1 && d.samples.size() == (size_t)N && mism == 0
                      && d.startTimeMs == 1704067200000LL;
    std::printf("%s\n", good ? "WRAPPER ROUNDTRIP OK" : "WRAPPER ROUNDTRIP FAILED");
    return good ? 0 : 1;
}
