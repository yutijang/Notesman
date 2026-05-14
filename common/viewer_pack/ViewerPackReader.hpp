#pragma once

#include "common/viewer_pack/ViewerPackError.hpp"
#include "common/viewer_pack/ViewerPackHeader.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>

class ViewerPackReader final {
  public:
    ViewerPackReader() = delete;
    ~ViewerPackReader() = default;

    ViewerPackReader(ViewerPackReader const&) = delete;
    ViewerPackReader& operator=(ViewerPackReader const&) = delete;

    ViewerPackReader(ViewerPackReader&&) noexcept = default;
    ViewerPackReader& operator=(ViewerPackReader&&) noexcept = default;

    [[nodiscard]]
    static std::expected<ViewerPackReader, ViewerPackError>
        read(std::filesystem::path const& filePath);

    [[nodiscard]]
    ViewerPackHeader const& header() const noexcept {
        return m_header;
    }

  private:
    explicit ViewerPackReader(ViewerPackHeader header) noexcept;

    static bool validateMagic(ViewerPackHeader const& header) noexcept;
    static bool validateVersion(ViewerPackHeader const& header) noexcept;
    static bool validateHeaderSize(ViewerPackHeader const& header) noexcept;
    static bool validateCrc(ViewerPackHeader const& header) noexcept;

    static std::uint32_t computeHeaderCrc32(ViewerPackHeader const& header) noexcept;

    ViewerPackHeader m_header{};
};
