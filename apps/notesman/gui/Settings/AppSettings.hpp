#pragma once

#include "gui/Settings/SettingsData.hpp"
#include "gui/UiConstants.hpp"

#include <filesystem>

class AppSettings {
  public:
    AppSettings() = default;

    bool load(std::filesystem::path const& path);
    [[nodiscard]] bool save(std::filesystem::path const& path) const;

    // Getter
    [[nodiscard]] UiConst::Theme theme() const noexcept {
        return m_theme;
    }

    [[nodiscard]] UiConst::Language language() const noexcept {
        return m_language;
    }

    [[nodiscard]]
    std::filesystem::path resourceDir() const noexcept {
        return m_resourceDir;
    }

    [[nodiscard]] bool isManagedResources() const noexcept {
        return m_isManagedResource;
    }

    [[nodiscard]]
    bool isDefaultResourceDir() const noexcept {
        return m_resourceDir == std::filesystem::path{K_DEFAULT_RESOURCE_DIR};
    }

    [[nodiscard]] bool isCleanupEpubCache() const noexcept {
        return m_isEpubCleanupCache;
    }

    [[nodiscard]] bool isCleanupMDCache() const noexcept {
        return m_isMDCleanupCache;
    }

    [[nodiscard]] int daysCleanupEpubCache() const noexcept {
        return m_expiredCleanupEpubCache;
    }

    [[nodiscard]] int daysCleanupMDCache() const noexcept {
        return m_expiredCleanupMDCache;
    }

    // maybe unused
    [[nodiscard]] bool isResourceDirCustomized() const noexcept {
        return m_isResourceDirCustomized;
    }

    // Setter
    void setTheme(UiConst::Theme theme) noexcept;
    void setLanguage(UiConst::Language language) noexcept;
    void setResourceDir(std::filesystem::path path) noexcept;
    void setManagedResources(bool managed) noexcept;
    void setResourceDirCustomized(bool customized) noexcept;
    void setCleanupEpubCache(bool isEnableCleanup) noexcept;
    void setCleanupMDCache(bool isEnableCleanup) noexcept;
    void setExpiredCleanupEpubCache(int days) noexcept;
    void setExpiredCleanupMDCache(int days) noexcept;

    // =====================

    void markDirty(bool dirty = true) noexcept {
        m_dirty = dirty;
    }

    [[nodiscard]] bool isDirty() const noexcept {
        return m_dirty;
    }

    [[nodiscard]] SettingsData toUiSettings() const;
    static SettingsData defaultUiSettings();

  private:
    static constexpr char const* K_DEFAULT_RESOURCE_DIR{"resources"};
    static constexpr int K_CACHE_EXPIRED{7};

    std::filesystem::path m_resourceDir{K_DEFAULT_RESOURCE_DIR};

    int m_expiredCleanupEpubCache{K_CACHE_EXPIRED};
    int m_expiredCleanupMDCache{K_CACHE_EXPIRED};

    UiConst::Theme m_theme{UiConst::Theme::Light};
    UiConst::Language m_language{UiConst::Language::English};
    bool m_isManagedResource{true};
    bool m_isResourceDirCustomized{};
    bool m_isEpubCleanupCache{true};
    bool m_isMDCleanupCache{true};

    bool m_dirty{}; // trạng thái thay đổi kể từ lần load/save cuối
};
