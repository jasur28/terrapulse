#pragma once
#include <cstdint>
#include <cstddef>

namespace tp {

uint32_t crc32(const void* data, size_t length, uint32_t crc = 0);

} // namespace tp
