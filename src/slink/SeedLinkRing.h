#pragma once
#include <cstdint>
#include <vector>

// SeedLinkRing — the retention buffer at the heart of a SeedLink server.
// It frames each 512-byte miniSEED record into a 520-byte SeedLink packet
// ("SL" + 6 hex sequence digits + the record) and keeps the most recent N.
//
// A reconnecting client asks for packets from a sequence number (DATA <seq>);
// the ring serves them until they age out, then reports the gap honestly rather
// than sending wrong data. Spec: docs/SEEDLINK-PROTOCOL.md.
namespace tp::slink {

constexpr int      kPacketSize = 520;
constexpr int      kHeaderSize = 8;         // "SL" + 6 hex sequence digits
constexpr int      kRecordSize = 512;
constexpr uint32_t kSeqMask    = 0xFFFFFF;  // 24-bit sequence, wraps like SeisComp

enum class ReadResult {
    Ok,       // packet copied
    TooOld,   // sequence already overwritten — client must resync
    Future,   // sequence not produced yet
};

class SeedLinkRing {
public:
    // capacityPackets = how many recent 520-byte packets to retain (>=1).
    explicit SeedLinkRing(std::size_t capacityPackets);

    // Frame a 512-byte record and store it under the next sequence number.
    // Returns the wire sequence assigned (24-bit).
    uint32_t push(const char* record512);

    // Copy the 520-byte packet for wire sequence `seq` into out[>=520].
    ReadResult packet(uint32_t seq, char* out) const;

    uint32_t   nextSeq()   const { return uint32_t(m_produced & kSeqMask); }   // next push gets this
    uint32_t   oldestSeq() const { return uint32_t((m_produced - m_count) & kSeqMask); } // oldest retained
    bool       empty()     const { return m_count == 0; }
    std::size_t retained() const { return m_count; }
    uint64_t   produced()  const { return m_produced; }   // absolute count, no wrap

    // Format a 24-bit sequence as the 6 uppercase hex digits used on the wire.
    static void writeSeqDigits(uint32_t seq, char out6[6]);

private:
    struct Slot { uint64_t abs; char data[kPacketSize]; };

    std::vector<Slot> m_slots;    // circular
    std::size_t       m_cap;
    std::size_t       m_head = 0; // next write slot
    std::size_t       m_count = 0;
    uint64_t          m_produced = 0;  // absolute number of packets ever pushed
};

} // namespace tp::slink
