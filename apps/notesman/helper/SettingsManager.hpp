#pragma once

#include <QSettings>
#include <QString>
#include <QVariant>
#include <mutex>

class SettingsManager {
  public:
    // Lấy instance singleton
    static SettingsManager& instance();

    // Xóa copy/move
    SettingsManager(SettingsManager const&) = delete;
    SettingsManager& operator=(SettingsManager const&) = delete;
    SettingsManager(SettingsManager&&) = delete;
    SettingsManager& operator=(SettingsManager&&) = delete;

    // --- Wrapper ---
    QVariant get(QString const& key, QVariant const& defaultValue = {}) const;

    void set(QString const& key, QVariant const& value);

    void remove(QString const& key);

    void clear();

  private:
    SettingsManager();

    QSettings m_qSettings;
    mutable std::mutex m_mutex;
};
