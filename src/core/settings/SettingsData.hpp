#pragma once

#include <filesystem>

#include "Language.hpp"
#include "Theme.hpp"

struct SettingsData {
        [[nodiscard]]
        bool isDefaultResourceDir() const noexcept {
            return resourceDir == std::filesystem::path{"resources"};
        }

        Theme theme{};
        Language language{};
        std::filesystem::path resourceDir;
        bool isManagedResource{};
        bool isResourceDirCustomized{};
};
