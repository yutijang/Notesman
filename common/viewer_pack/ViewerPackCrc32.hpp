#pragma once

#include <cstddef>
#include <cstdint>

// CRC32 implementation (IEEE 802.3, reflected)
std::uint32_t computeCrc32(void const* data, std::size_t size);
