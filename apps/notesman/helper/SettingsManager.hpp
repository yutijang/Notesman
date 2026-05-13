#pragma once

#include "app_version.hpp"

#include <QSettings>
#include <QString>
#include <QVariant>
#include <mutex>

class SettingsManager {
  public:
    // Lấy instance singleton
    static SettingsManager& instance() {
        static auto* sInstance = new SettingsManager(); // intentional leak, no exit-time destructor
        return *sInstance;
    }

    // Xóa copy/move
    SettingsManager(SettingsManager const&) = delete;
    SettingsManager& operator=(SettingsManager const&) = delete;
    SettingsManager(SettingsManager&&) = delete;
    SettingsManager& operator=(SettingsManager&&) = delete;

    // --- Wrapper ---
    QVariant get(QString const& key, QVariant const& defaultValue = {}) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_qSettings.value(key, defaultValue);
    }

    void set(QString const& key, QVariant const& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_qSettings.setValue(key, value);
    }

    void remove(QString const& key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_qSettings.remove(key);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_qSettings.clear();
    }

  private:
    SettingsManager() : m_qSettings(app::meta::NAME, QStringLiteral("configs")) {}

    QSettings m_qSettings;
    mutable std::mutex m_mutex;
};
