// test_seedlinkring — the SeedLink retention buffer (Stage 5, slice 1).
// Locks the framing and the resume contract a reconnecting client depends on:
// a packet in the window comes back byte-exact; one that aged out reports
// TooOld (never wrong data); one not yet produced reports Future.

#include "slink/SeedLinkRing.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace tp::slink;

static int failures = 0;
static void check(bool ok, const char* what, const std::string& d = {}) {
    std::printf("%-4s %-42s %s\n", ok ? "ok" : "FAIL", what, d.c_str());
    if (!ok) ++failures;
}

// A 512-byte record whose first bytes encode `n`, so we can prove identity.
static void makeRecord(int n, char rec[kRecordSize]) {
    std::memset(rec, 0, kRecordSize);
    std::snprintf(rec, kRecordSize, "REC-%d", n);
}

int main() {
    SeedLinkRing ring(4);   // retain 4 packets

    // Empty ring: any request is Future.
    char buf[kPacketSize];
    check(ring.packet(0, buf) == ReadResult::Future, "empty ring -> Future");
    check(ring.nextSeq() == 0, "first sequence is 0");

    // Push 3 records; sequences 0,1,2 assigned.
    for (int i = 0; i < 3; ++i) { char r[kRecordSize]; makeRecord(i, r); ring.push(r); }
    check(ring.retained() == 3, "retained 3");
    check(ring.nextSeq() == 3, "nextSeq is 3");

    // Read seq 1 back: framing "SL000001" + "REC-1".
    check(ring.packet(1, buf) == ReadResult::Ok, "read seq 1 -> Ok");
    check(std::memcmp(buf, "SL000001", 8) == 0, "header is SL + 6 hex",
          std::string(buf, 8));
    check(std::strcmp(buf + kHeaderSize, "REC-1") == 0, "payload is REC-1");

    // Not yet produced.
    check(ring.packet(3, buf) == ReadResult::Future, "read seq 3 -> Future");

    // Overflow the capacity-4 ring: push 0..7, so 0..3 age out, 4..7 retained.
    SeedLinkRing r2(4);
    for (int i = 0; i < 8; ++i) { char r[kRecordSize]; makeRecord(i, r); r2.push(r); }
    check(r2.retained() == 4, "retained capped at 4");
    check(r2.produced() == 8, "produced counts all 8");

    check(r2.packet(2, buf) == ReadResult::TooOld, "aged-out seq 2 -> TooOld");
    check(r2.packet(4, buf) == ReadResult::Ok, "oldest retained seq 4 -> Ok");
    check(r2.packet(7, buf) == ReadResult::Ok, "newest seq 7 -> Ok");
    if (r2.packet(7, buf) == ReadResult::Ok)
        check(std::strcmp(buf + kHeaderSize, "REC-7") == 0, "seq 7 payload REC-7");
    check(r2.packet(8, buf) == ReadResult::Future, "next seq 8 -> Future");

    // Hex framing for a larger sequence.
    char d6[6]; SeedLinkRing::writeSeqDigits(0xABCDE, d6);
    check(std::memcmp(d6, "0ABCDE", 6) == 0, "seq 0xABCDE -> 0ABCDE",
          std::string(d6, 6));

    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
                failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
