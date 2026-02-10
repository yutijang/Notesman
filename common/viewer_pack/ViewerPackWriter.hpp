#pragma once

#include <cstdint>
#include <filesystem>
#include <expected>

#include "ViewerPackHeader.hpp"
#include "ViewerPackError.hpp"

class ViewerPackWriter final {
    public:
        ViewerPackWriter() = default;

        ViewerPackWriter(const ViewerPackWriter &) = delete;
        ViewerPackWriter &operator=(const ViewerPackWriter &) = delete;

        [[nodiscard]]
        std::expected<void, ViewerPackError> write(const std::filesystem::path &filePath,
                                                   ViewerPackHeader header) const;

    private:
        static std::uint32_t computeHeaderCrc32(const ViewerPackHeader &header);
};
