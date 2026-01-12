#include <memory>
#include <QApplication>
#include <QTabWidget>
#include <QCoreApplication>
#include <QObject>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "MainWindow.hpp"

// Helper để quản lý vòng đời QApplication trong test
struct QtTestFixture {
        QtTestFixture() {
            static int argc = 1;
            static char arg0[] = "test_app";
            static char* argv[] = {arg0, nullptr};
            if (!qApp) { app = std::make_unique<QApplication>(argc, argv); }
        }

        std::unique_ptr<QApplication> app;
};

TEST_CASE("MainWindow Tab Structure Check", "[GUI][MainWindow][Structure]") {
    // Tạo instance của MainWindow
    MainWindow window;

    // Lấy widget trung tâm (phải là QTabWidget)
    auto* central = window.centralWidget();
    auto* tabWidget = qobject_cast<QTabWidget*>(central);

    SECTION("Central Widget Check") {
        // Kiểm tra xem widget trung tâm có tồn tại và là QTabWidget hay không
        REQUIRE(tabWidget != nullptr);
    }

    SECTION("Tab Count Check") {
        // Kiểm tra số lượng tab (Browse, Add Note, Settings)
        REQUIRE(tabWidget->count() == 3);
    }

    SECTION("Tab Title and Initial State Check") {
        // Ta sử dụng window.tr("Title") để lấy chuỗi đã được dịch (trong môi trường test mặc định
        // là Anh ngữ).

        // Tab 0: Browse
        REQUIRE(tabWidget->tabText(0) == window.tr("Browse"));
        // Trong MainWindow.cpp, tab Browse được setTabEnabled(false) ban đầu.
        REQUIRE(tabWidget->isTabEnabled(0) == true);

        // Tab 1: Add Note
        REQUIRE(tabWidget->tabText(1) == window.tr("Add Notes"));
        // Trong MainWindow.cpp, tab Add Note được setTabEnabled(false) ban đầu.
        REQUIRE(tabWidget->isTabEnabled(1) == true);

        // Tab 2: Settings
        REQUIRE(tabWidget->tabText(2) == window.tr("Settings"));
        // Tab Settings không bị disable rõ ràng trong MainWindow.cpp, mặc định là true.
        REQUIRE(tabWidget->isTabEnabled(2) == true);
    }

    // Lưu ý: MainWindow bị xóa khi ra khỏi scope của TEST_CASE.
}
