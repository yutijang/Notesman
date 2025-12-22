#pragma once

#include <QSettings>
#include <QString>
#include <QVariant>
#include <mutex>

class SettingsManager {
    public:
        // Lấy instance singleton
        static SettingsManager &instance() {
            static auto* sInstance =
                new SettingsManager(); // intentional leak, no exit-time destructor
            return *sInstance;
        }

        // Xóa copy/move
        SettingsManager(const SettingsManager &) = delete;
        SettingsManager &operator=(const SettingsManager &) = delete;
        SettingsManager(SettingsManager &&) = delete;
        SettingsManager &operator=(SettingsManager &&) = delete;

        // --- Wrapper ---
        QVariant get(const QString &key, const QVariant &defaultValue = {}) const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_settings.value(key, defaultValue);
        }

        void set(const QString &key, const QVariant &value) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_settings.setValue(key, value);
        }

        void remove(const QString &key) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_settings.remove(key);
        }

        void clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_settings.clear();
        }

    private:
        SettingsManager() : m_settings(QStringLiteral("Notesman"), QStringLiteral("configs")) {}

        QSettings m_settings;
        mutable std::mutex m_mutex;
};
