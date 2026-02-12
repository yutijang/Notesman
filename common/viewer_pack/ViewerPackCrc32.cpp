#include <cstdint>
#include <cstddef>

#include "ViewerPackCrc32.hpp"

// CRC32 implementation (IEEE 802.3, reflected)
std::uint32_t computeCrc32(const void* data, std::size_t size) {
    constexpr std::uint32_t poly = 0xEDB88320U;
    constexpr std::uint32_t initialXor = 0xFFFFFFFFU;

    std::uint32_t crc = initialXor;

    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) { crc = (crc >> 1) ^ (((crc & 1U) != 0U) ? poly : 0U); }
    }

    return ~crc;
}
