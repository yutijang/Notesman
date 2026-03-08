#pragma once

#include "UiConstants.hpp"

#include <filesystem>

struct SettingsData {
        [[nodiscard]]
        bool isDefaultResourceDir() const noexcept {
            return resourceDir == std::filesystem::path{"resources"};
        }

        UiConst::Theme theme{};
        UiConst::Language language{};
        std::filesystem::path resourceDir;
        bool isManagedResource{};
        bool isResourceDirCustomized{};
        bool isEpubCleanupCache{};
        bool isMDCleanupCache{};
        int expiredCleanupEpubCache{};
        int expiredCleanupMDCache{};
};
