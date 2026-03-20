find_package(Qt6 COMPONENTS Test Widgets REQUIRED)

# ------------------------------
# GUI Tests
# ------------------------------
add_executable(gui-tests
    test_main.cpp
    test_mainwindow_tabs.cpp
    test_AppSettings.cpp
)

target_link_libraries(gui-tests
    PRIVATE
        notes-app-lib-core         # Thư viện logic chính (app + gui)
        notes-app-lib-qtkeychain
        Catch2::Catch2  # Cần có main() và các macro test
        Qt6::Test             # Cần thiết cho các tính năng test của Qt
        Qt6::Widgets          # Cần thiết nếu test tương tác với QWidgets (gần như chắc chắn)
)

set_target_properties(gui-tests PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)

# add_test(NAME gui-tests COMMAND $<TARGET_FILE:gui-tests>)
catch_discover_tests(gui-tests
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
