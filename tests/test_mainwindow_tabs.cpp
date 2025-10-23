#include "MainWindow.hpp"
#include <QApplication>
#include <QTabWidget>
#include <QCoreApplication>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

// Lưu ý: Đối với các bài test GUI, hàm main() của test executable
// (thường là trong test_main.cpp) phải đảm bảo khởi tạo một instance QApplication
// trước khi chạy bất kỳ TEST_CASE nào.

// Giả định: Các class dependency như NotesAppCore, AppController không cần
// được khởi tạo hoặc được thay thế bằng mock/nullptr vì ta chỉ test cấu trúc
// của MainWindow.

TEST_CASE("MainWindow Tab Structure Check", "[GUI][MainWindow][Structure]") {
    // Bước 1: Khởi tạo QApplication
    // Ta sử dụng QCoreApplication::instance() để kiểm tra nếu QApplication đã được
    // khởi tạo. Nếu test của bạn chạy trong môi trường đã khởi tạo QApplication
    // (do Qt6::Test hoặc test_main.cpp), dòng này sẽ an toàn.
    if (QCoreApplication::instance() == nullptr) {
        static int argc = 0;
        static char* argv[] = {nullptr}; // NOLINT(modernize-avoid-c-arrays)
        // Sử dụng QApplication thay vì QCoreApplication vì MainWindow là QMainWindow
        new QApplication(argc, argv);
    }

    // Tạo instance của MainWindow
    MainWindow window;

    // Lấy widget trung tâm (phải là QTabWidget)
    auto* tabWidget = qobject_cast<QTabWidget*>(window.centralWidget());

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
        REQUIRE(tabWidget->isTabEnabled(0) == false);

        // Tab 1: Add Note
        REQUIRE(tabWidget->tabText(1) == window.tr("Add Note"));
        // Trong MainWindow.cpp, tab Add Note được setTabEnabled(false) ban đầu.
        REQUIRE(tabWidget->isTabEnabled(1) == false);

        // Tab 2: Settings
        REQUIRE(tabWidget->tabText(2) == window.tr("Settings"));
        // Tab Settings không bị disable rõ ràng trong MainWindow.cpp, mặc định là true.
        REQUIRE(tabWidget->isTabEnabled(2) == true);
    }

    // Lưu ý: MainWindow bị xóa khi ra khỏi scope của TEST_CASE.
}