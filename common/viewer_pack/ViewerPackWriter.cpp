#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <ios>
#include <system_error>

#include "ViewerPackWriter.hpp"
#include "ViewerPackError.hpp"
#include "ViewerPackHeader.hpp"

// CRC32 implementation (IEEE 802.3, reflected)
static std::uint32_t crc32(const void* data, std::size_t size) {
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

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
std::expected<void, ViewerPackError> ViewerPackWriter::write(const std::filesystem::path &filePath,
                                                             ViewerPackHeader header) const {
    std::filesystem::path tempPath = filePath;
    tempPath.replace_extension(filePath.extension().string() + ".tmp");

    {
        std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) { return std::unexpected(ViewerPackError::FileOpenFailed); }

        header.headerSize = static_cast<std::uint16_t>(sizeof(ViewerPackHeader));
        header.crc32 = computeHeaderCrc32(header);

        ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
        if (!ofs.good()) {
            std::filesystem::remove(tempPath);
            return std::unexpected(ViewerPackError::FileOpenFailed);
        }
        ofs.flush();
    }

    std::error_code ec;
    std::filesystem::rename(tempPath, filePath, ec);

    if (ec) {
        if (ec.value() == static_cast<int>(std::errc::permission_denied)) {
            std::filesystem::remove(tempPath);
            return std::unexpected(ViewerPackError::FileOpenFailed);
        }

        std::filesystem::remove(tempPath);
        return std::unexpected(ViewerPackError::FileOpenFailed);
    }

    return {};
}

std::uint32_t ViewerPackWriter::computeHeaderCrc32(const ViewerPackHeader &header) {
    // CRC over header[0 .. crc32 field)
    constexpr std::size_t crcOffset = offsetof(ViewerPackHeader, crc32);

    return crc32(&header, crcOffset);
}
