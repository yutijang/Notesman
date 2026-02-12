#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <cstddef>
#include <cstring>
#include <ios>
#include <string_view>

#include "ViewerPackReader.hpp"
#include "ViewerPackError.hpp"
#include "ViewerPackHeader.hpp"
#include "ViewerPackCrc32.hpp"

// ---------------------------------------------------------------------

ViewerPackReader::ViewerPackReader(ViewerPackHeader header) noexcept : m_header(header) {}

// ---------------------------------------------------------------------

std::expected<ViewerPackReader, ViewerPackError>
    ViewerPackReader::read(const std::filesystem::path &filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) { return std::unexpected(ViewerPackError::FileOpenFailed); }

    ViewerPackHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (file.gcount() != sizeof(header)) {
        return std::unexpected(ViewerPackError::FileSizeMismatch);
    }

    if (!validateMagic(header)) { return std::unexpected(ViewerPackError::MagicMismatch); }

    if (!validateVersion(header)) { return std::unexpected(ViewerPackError::UnsupportedVersion); }

    if (!validateHeaderSize(header)) {
        return std::unexpected(ViewerPackError::HeaderSizeMismatch);
    }

    if (!validateCrc(header)) { return std::unexpected(ViewerPackError::CrcMismatch); }

    return ViewerPackReader{header};
}

// ---------------------------------------------------------------------

bool ViewerPackReader::validateMagic(const ViewerPackHeader &header) noexcept {
    return std::string_view(header.magic, 4) == std::string_view(ViewerPackHeader::RVPK_MAGIC, 4);
}

bool ViewerPackReader::validateVersion(const ViewerPackHeader &header) noexcept {
    return header.version == ViewerPackHeader::VERSION;
}

bool ViewerPackReader::validateHeaderSize(const ViewerPackHeader &header) noexcept {
    return header.headerSize == sizeof(ViewerPackHeader);
}

bool ViewerPackReader::validateCrc(const ViewerPackHeader &header) noexcept {
    return header.crc32 == computeHeaderCrc32(header);
}

std::uint32_t ViewerPackReader::computeHeaderCrc32(const ViewerPackHeader &header) noexcept {
    constexpr std::size_t crcOffset = offsetof(ViewerPackHeader, crc32);

    return computeCrc32(&header, crcOffset);
}
