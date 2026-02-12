#pragma once

#include <cstdint>
#include <cstddef>

// CRC32 implementation (IEEE 802.3, reflected)
std::uint32_t computeCrc32(const void* data, std::size_t size);
