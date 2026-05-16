#include "helper/SettingsManager.hpp"

#include "app_version.hpp"

#include <QString>
#include <QVariant>
#include <mutex>

SettingsManager& SettingsManager::instance() {
    static auto* sInstance = new SettingsManager(); // intentional leak, no exit-time destructor
    return *sInstance;
}

QVariant SettingsManager::get(QString const& key, QVariant const& defaultValue) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_qSettings.value(key, defaultValue);
}

void SettingsManager::set(QString const& key, QVariant const& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_qSettings.setValue(key, value);
}

void SettingsManager::remove(QString const& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_qSettings.remove(key);
}

void SettingsManager::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_qSettings.clear();
}

SettingsManager::SettingsManager() : m_qSettings(app::meta::NAME, QStringLiteral("configs")) {}
