# ---------------------------------------------------------
# Unit Tests for notes-core using Catch2 v3
# ---------------------------------------------------------

# Bật testing cho toàn project (chỉ cần gọi một lần trong root/subdir tests)
# enable_testing()

# Tìm Catch2 v3 được cài qua vcpkg
find_package(Catch2 CONFIG REQUIRED)
find_package(Qt6 COMPONENTS Test Widgets REQUIRED)

# Tạo executable cho test
add_executable(notes-core-tests
    test_main.cpp
    test_AppSettings.cpp
    test_model_utils.cpp
    test_sqldb_raii.cpp
    test_resource_repository.cpp
    test_text_content_repository.cpp
    test_tag_repository.cpp
    test_file_repository.cpp
    test_resource_service.cpp
    test_file_service.cpp
)

# Include các thư mục header để test thấy được API của notes-core
target_include_directories(notes-core-tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/core
)

# Liên kết với notes-core và Catch2
target_link_libraries(notes-core-tests
    PRIVATE
        notes-core              # thư viện logic chính
        Catch2::Catch2WithMain  # target có sẵn main() của Catch2 v3
)

# Đảm bảo test build bằng chuẩn C++23 như project chính
set_target_properties(notes-core-tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/tests"
)

# ------------------------------
# GUI Tests
# ------------------------------
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

add_executable(gui-tests
    test_mainwindow_tabs.cpp
)

# Thêm đường dẫn include để thấy header của notes-app-lib
target_include_directories(gui-tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src  # Đường dẫn đến app, gui, helper
        ${PROJECT_SOURCE_DIR}/src/core/db
        ${PROJECT_SOURCE_DIR}/src/core/model
        ${PROJECT_SOURCE_DIR}/src/core/repository
        ${PROJECT_SOURCE_DIR}/src/core/service
        ${PROJECT_SOURCE_DIR}/src/core/settings 
        ${PROJECT_SOURCE_DIR}/src/helper        
)

target_link_libraries(gui-tests
    PRIVATE
        notes-app-lib         # Thư viện logic chính (app + gui)
        Catch2::Catch2WithMain  # Cần có main() và các macro test
        Qt6::Test             # Cần thiết cho các tính năng test của Qt
        Qt6::Widgets          # Cần thiết nếu test tương tác với QWidgets (gần như chắc chắn)
)

set_target_properties(gui-tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/tests"
)

add_test(NAME gui-tests COMMAND $<TARGET_FILE:gui-tests>)
# ------------------------------

# Tự động phát hiện TEST_CASE() của Catch2
include(CTest)
include(Catch)
catch_discover_tests(notes-core-tests)
