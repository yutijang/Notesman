#include "ViewerPackWriter.hpp"

#include "ViewerPackCrc32.hpp"
#include "ViewerPackError.hpp"
#include "ViewerPackHeader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <system_error>

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
std::expected<void, ViewerPackError> ViewerPackWriter::write(std::filesystem::path const& filePath,
                                                             ViewerPackHeader header) const {
    std::filesystem::path tempPath = filePath;
    tempPath.replace_extension(filePath.extension().string() + ".tmp");

    {
        std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) { return std::unexpected(ViewerPackError::FileOpenFailed); }

        header.headerSize = static_cast<std::uint16_t>(sizeof(ViewerPackHeader));
        header.crc32 = computeHeaderCrc32(header);

        ofs.write(reinterpret_cast<char const*>(&header), sizeof(header));
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

std::uint32_t ViewerPackWriter::computeHeaderCrc32(ViewerPackHeader const& header) {
    // CRC over header[0 .. crc32 field)
    constexpr std::size_t crcOffset = offsetof(ViewerPackHeader, crc32);

    return computeCrc32(&header, crcOffset);
}
