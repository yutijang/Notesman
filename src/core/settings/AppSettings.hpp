#pragma once

#include <filesystem>

#include "Theme.hpp"
#include "Language.hpp"
#include "SettingsData.hpp"

class AppSettings {
    public:
        AppSettings() = default;

        bool load(const std::filesystem::path &path);
        [[nodiscard]] bool save(const std::filesystem::path &path) const;

        // Getter
        [[nodiscard]] Theme theme() const noexcept { return m_theme; }

        [[nodiscard]] Language language() const noexcept { return m_language; }

        [[nodiscard]] std::filesystem::path resourceDir() const noexcept { return m_resourceDir; }

        [[nodiscard]] bool isManagedResources() const noexcept { return m_isManagedResource; }

        [[nodiscard]] bool isDefaultResourceDir() const noexcept {
            return m_resourceDir == std::filesystem::path{K_DEFAULT_RESOURCE_DIR};
        }

        // Setter
        void setTheme(Theme theme) noexcept;
        void setLanguage(Language language) noexcept;
        void setResourceDir(std::filesystem::path path) noexcept;
        void setManagedResources(bool managed) noexcept;

        // =====================

        void markDirty(bool dirty = true) noexcept { m_dirty = dirty; }

        [[nodiscard]] bool isDirty() const noexcept { return m_dirty; }

        [[nodiscard]] SettingsData toUiSettings() const;
        static SettingsData defaultUiSettings();

    private:
        static constexpr const char* K_DEFAULT_RESOURCE_DIR = "resources";

        Theme m_theme{Theme::light};
        Language m_language{Language::english};
        std::filesystem::path m_resourceDir{K_DEFAULT_RESOURCE_DIR};
        bool m_isManagedResource{true};
        bool m_isResourceDirCustomized{};

        bool m_dirty{}; // trạng thái thay đổi kể từ lần load/save cuối
};
