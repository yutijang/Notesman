#pragma once

#include <filesystem>

#include "UiConstants.hpp"

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
};
