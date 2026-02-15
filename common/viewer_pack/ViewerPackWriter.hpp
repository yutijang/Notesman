#pragma once

#include <cstdint>
#include <filesystem>
#include <expected>

#include "ViewerPackHeader.hpp"
#include "ViewerPackError.hpp"

class ViewerPackWriter final {
    public:
        ViewerPackWriter() = default;

        ViewerPackWriter(ViewerPackWriter const &) = delete;
        ViewerPackWriter &operator=(ViewerPackWriter const &) = delete;

        [[nodiscard]]
        std::expected<void, ViewerPackError> write(std::filesystem::path const &filePath,
                                                   ViewerPackHeader header) const;

    private:
        [[nodiscard]]
        static std::uint32_t computeHeaderCrc32(ViewerPackHeader const &header);
};
