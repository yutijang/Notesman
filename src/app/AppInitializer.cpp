#include <memory>
#include "AppInitializer.hpp"
#include "MainWindow.hpp"
#include "AppController.hpp"
#include "FontLoader.hpp"

AppInitializer::AppInitializer() = default;
AppInitializer::~AppInitializer() = default;

void AppInitializer::run() {
    FontLoader::loadCustomFontOnce();

    // 1. Tạo controller và window
    m_controller = std::make_unique<AppController>();
    m_mainWindow = std::make_unique<MainWindow>();

    m_controller->setMainWindow(m_mainWindow.get());

    // 2. Kết nối giữa GUI và logic ứng dụng
    m_mainWindow->setAppController(m_controller.get());
    AppController::setupConnections(*m_mainWindow, *m_controller);

    // 3. Nạp cấu hình từ file (nếu có)
    m_controller->loadSettings();
    const AppSettings* settings = m_controller->settings();

    // 4. Áp dụng ngôn ngữ trước khi hiển thị GUI
    if (settings != nullptr) {
        m_controller->applyLanguage(settings->language());
        // (tùy chọn) Áp dụng theme ở đây
        m_controller->applyTheme(settings->theme());
    }

    // 5. Đồng bộ lại giao diện Settings tab
    m_mainWindow->applySettingsToUi(settings);

    // 6. Hiển thị giao diện
    m_mainWindow->show();
}
