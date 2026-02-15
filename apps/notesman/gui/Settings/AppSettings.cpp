#include <filesystem>
#include <fstream>
#include <ios>
#include <unordered_map>
#include <string>
#include <utility>

#include "AppSettings.hpp"
#include "SettingsData.hpp"
#include "UiConstants.hpp"

bool AppSettings::load(std::filesystem::path const& path) {
    if (!std::filesystem::exists(path)) { return false; }

    std::ifstream file(path);
    if (!file.is_open()) { return false; }

    std::unordered_map<std::string, std::string> kv;
    std::string line;
    while (std::getline(file, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) { continue; }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        kv[key] = value;
    }

    if (kv.contains("theme")) {
        m_theme = (kv["theme"] == "dark") ? UiConst::Theme::Dark : UiConst::Theme::Light;
    }
    if (kv.contains("language")) {
        m_language =
            (kv["language"] == "vi") ? UiConst::Language::Vietnamese : UiConst::Language::English;
    }
    if (kv.contains("resource_dir")) {
        // Ép kiểu dữ liệu về chuỗi UTF-8 trước khi đưa vào path
        std::string rawValue = kv["resource_dir"];
        m_resourceDir = std::filesystem::path(std::u8string(rawValue.begin(), rawValue.end()));
    }
    if (kv.contains("is_managed")) {
        m_isManagedResource = (kv["is_managed"] == "true" || kv["is_managed"] == "1");
    }
    if (kv.contains("resource_dir_is_default")) {
        m_isResourceDirCustomized =
            (kv["resource_dir_is_default"] == "true" || kv["resource_dir_is_default"] == "1");
    }
    if (kv.contains("is_cleanup_epub_cache")) {
        m_isEpubCleanupCache =
            (kv["is_cleanup_epub_cache"] == "true" || kv["is_cleanup_epub_cache"] == "1");
    }
    if (kv.contains("is_cleanup_md_cache")) {
        m_isMDCleanupCache =
            (kv["is_cleanup_md_cache"] == "true" || kv["is_cleanup_md_cache"] == "1");
    }
    if (kv.contains("expired_cleanup_epub_cache")) {
        m_expiredCleanupEpubCache = std::stoi(kv["expired_cleanup_epub_cache"]);
    }
    if (kv.contains("expired_cleanup_md_cache")) {
        m_expiredCleanupMDCache = std::stoi(kv["expired_cleanup_md_cache"]);
    }

    m_dirty = false;

    return true;
}

bool AppSettings::save(std::filesystem::path const& path) const {
    if (!m_dirty) { return true; }

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) { return false; }

    file << "theme=" << (m_theme == UiConst::Theme::Light ? "light" : "dark") << "\n";
    file << "language=" << (m_language == UiConst::Language::English ? "en" : "vi") << "\n";
    file << "resource_dir=" << m_resourceDir.string() << "\n";
    file << "is_managed=" << (m_isManagedResource ? "true" : "false") << "\n";
    file << "resource_dir_is_default=" << (m_isResourceDirCustomized ? "true" : "false") << "\n";
    file << "is_cleanup_epub_cache=" << (m_isEpubCleanupCache ? "true" : "false") << "\n";
    file << "is_cleanup_md_cache=" << (m_isMDCleanupCache ? "true" : "false") << "\n";
    file << "expired_cleanup_epub_cache=" << m_expiredCleanupEpubCache << "\n";
    file << "expired_cleanup_md_cache=" << m_expiredCleanupMDCache << "\n";

    return true;
}

void AppSettings::setTheme(UiConst::Theme theme) noexcept {
    if (m_theme != theme) {
        m_theme = theme;
        m_dirty = true;
    }
}

void AppSettings::setLanguage(UiConst::Language language) noexcept {
    if (m_language != language) {
        m_language = language;
        m_dirty = true;
    }
}

void AppSettings::setResourceDir(std::filesystem::path path) noexcept {
    if (m_resourceDir != path) {
        m_resourceDir = std::move(path);
        m_dirty = true;
    }
}

void AppSettings::setManagedResources(bool managed) noexcept {
    if (m_isManagedResource != managed) {
        m_isManagedResource = managed;
        m_dirty = true;
    }
}

void AppSettings::setResourceDirCustomized(bool customized) noexcept {
    if (m_isResourceDirCustomized != customized) {
        m_isResourceDirCustomized = customized;
        m_dirty = true;
    }
}

void AppSettings::setCleanupEpubCache(bool isEnableCleanup) noexcept {
    if (m_isEpubCleanupCache != isEnableCleanup) {
        m_isEpubCleanupCache = isEnableCleanup;
        m_dirty = true;
    }
}

void AppSettings::setCleanupMDCache(bool isEnableCleanup) noexcept {
    if (m_isMDCleanupCache != isEnableCleanup) {
        m_isMDCleanupCache = isEnableCleanup;
        m_dirty = true;
    }
}

void AppSettings::setExpiredCleanupEpubCache(int days) noexcept {
    if (m_expiredCleanupEpubCache != days) {
        m_expiredCleanupEpubCache = days;
        m_dirty = true;
    }
}

void AppSettings::setExpiredCleanupMDCache(int days) noexcept {
    if (m_expiredCleanupMDCache != days) {
        m_expiredCleanupMDCache = days;
        m_dirty = true;
    }
}

SettingsData AppSettings::toUiSettings() const {
    return {
        .theme = m_theme,
        .language = m_language,
        .resourceDir = m_resourceDir,
        .isManagedResource = m_isManagedResource,
        .isResourceDirCustomized = m_isResourceDirCustomized,
        .isEpubCleanupCache = m_isEpubCleanupCache,
        .isMDCleanupCache = m_isMDCleanupCache,
        .expiredCleanupEpubCache = m_expiredCleanupEpubCache,
        .expiredCleanupMDCache = m_expiredCleanupMDCache,
    };
}

SettingsData AppSettings::defaultUiSettings() {
    AppSettings const defaults{};
    return defaults.toUiSettings();
}
