#pragma once

#include "gui/UiConstants.hpp"

#include <filesystem>

struct SettingsData {
    [[nodiscard]]
    bool isDefaultResourceDir() const noexcept {
        return resourceDir == std::filesystem::path{"resources"};
    }

    std::filesystem::path resourceDir;
    int expiredCleanupEpubCache{};
    int expiredCleanupMDCache{};
    UiConst::Theme theme{};
    UiConst::Language language{};
    bool isManagedResource{};
    bool isResourceDirCustomized{};
    bool isEpubCleanupCache{};
    bool isMDCleanupCache{};
};
