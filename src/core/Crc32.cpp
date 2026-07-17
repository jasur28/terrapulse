#include "core/Crc32.h"

namespace tp {

static uint32_t buildTable(uint32_t n) {
    uint32_t c = n;
    for (int k = 0; k < 8; ++k)
        c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
    return c;
}

static const auto& crcTable() {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i)
            table[i] = buildTable(i);
        init = true;
    }
    return table;
}

uint32_t crc32(const void* data, size_t length, uint32_t crc) {
    const auto& table = crcTable();
    const auto* buf = static_cast<const uint8_t*>(data);
    crc ^= 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i)
        crc = table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

} // namespace tp
