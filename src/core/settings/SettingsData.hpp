#pragma once

#include <filesystem>

#include "Language.hpp"
#include "Theme.hpp"

struct SettingsData {
        Theme theme{};
        Language language{};
        std::filesystem::path resourceDir;
        bool isManagedResource{};
};
