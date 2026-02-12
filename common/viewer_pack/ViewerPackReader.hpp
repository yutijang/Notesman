#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>

#include "ViewerPackHeader.hpp"
#include "ViewerPackError.hpp"

class ViewerPackReader final {
    public:
        ViewerPackReader() = delete;
        ~ViewerPackReader() = default;

        ViewerPackReader(const ViewerPackReader &) = delete;
        ViewerPackReader &operator=(const ViewerPackReader &) = delete;

        ViewerPackReader(ViewerPackReader &&) noexcept = default;
        ViewerPackReader &operator=(ViewerPackReader &&) noexcept = default;

        [[nodiscard]]
        static std::expected<ViewerPackReader, ViewerPackError>
            read(const std::filesystem::path &filePath);

        [[nodiscard]]
        const ViewerPackHeader &header() const noexcept {
            return m_header;
        }

    private:
        explicit ViewerPackReader(ViewerPackHeader header) noexcept;

        static bool validateMagic(const ViewerPackHeader &header) noexcept;
        static bool validateVersion(const ViewerPackHeader &header) noexcept;
        static bool validateHeaderSize(const ViewerPackHeader &header) noexcept;
        static bool validateCrc(const ViewerPackHeader &header) noexcept;

        static std::uint32_t computeHeaderCrc32(const ViewerPackHeader &header) noexcept;

        ViewerPackHeader m_header{};
};
