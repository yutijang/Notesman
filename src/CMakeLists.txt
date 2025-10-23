set(CMAKE_PCH_WARN_INVALID ON)

# =========================================================
# Core library
# =========================================================
add_library(notes-core STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/repository/resource_repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/repository/text_content_repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/repository/file_repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/repository/tag_repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/service/file_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/service/resource_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/settings/AppSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/db/database_checker.cpp
)

target_include_directories(notes-core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/db
    ${CMAKE_CURRENT_SOURCE_DIR}/core/model
    ${CMAKE_CURRENT_SOURCE_DIR}/core/repository
    ${CMAKE_CURRENT_SOURCE_DIR}/core/service
    ${CMAKE_CURRENT_SOURCE_DIR}/core/settings 
    ${CMAKE_CURRENT_SOURCE_DIR}/helper
)

target_precompile_headers(notes-core
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/pch_stl.hpp
)

target_link_libraries(notes-core
  PRIVATE
    sqlite3_wrapper
  PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
)

# =========================================================
# Qt setup
# =========================================================
# Bật Qt MOC/UIC/RCC cho toàn project
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# =========================================================
# Sources
# =========================================================
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/app/NotesAppCore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/AppController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/AppInitializer.cpp
)

set(GUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/MainWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Widgets/BrowseTabWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Widgets/AddTabWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Highlighter/cpphighlighter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Highlighter/CodeEditorLineHighlighter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/TagInput/TagInput.cpp
)

set(HELPER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/FontLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/ResultsTable.cpp
)

# =========================================================
# notes-app-lib (App + GUI)
# =========================================================
add_library(notes-app-lib STATIC
    ${APP_SOURCES}
    ${GUI_SOURCES}
    ${HELPER_SOURCES}
)

target_include_directories(notes-app-lib
  SYSTEM PRIVATE
    ${PROJECT_SOURCE_DIR}/extern/sqlite3
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/settings
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/gui
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Highlighter
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/TagInput
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/helper    
)

target_precompile_headers(notes-app-lib
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/pch_qt.hpp
)

target_link_libraries(notes-app-lib
    PUBLIC
      notes-core
      Qt6::Core
      Qt6::Gui
      Qt6::Widgets
      OpenSSL::SSL
      OpenSSL::Crypto
)

# =========================================================
# Executable (GUI entry point)
# =========================================================
add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp    
    ${PROJECT_SOURCE_DIR}/resources/resources.qrc
)

# main.cpp chỉ cần liên kết với notes-app-lib
# Transitivity sẽ đảm bảo Qt và notes-core được liên kết
target_link_libraries(${PROJECT_NAME}
    PRIVATE
      notes-app-lib
)

target_precompile_headers(${PROJECT_NAME} REUSE_FROM notes-app-lib)

# Output dir
set_target_properties(${PROJECT_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
)

# Windows only: copy sqlite3.dll
if(WIN32)
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/extern/sqlite3/sqlite3.dll
          $<TARGET_FILE_DIR:${PROJECT_NAME}>
  )
endif()




#[[

add_executable(${PROJECT_NAME}
    main.cpp
    resource_repository.cpp
    text_content_repository.cpp
    file_repository.cpp
    tag_repository.cpp
    file_service.cpp
    resource_service.cpp
    )

# Thư mục include SQLite
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
  ${PROJECT_SOURCE_DIR}/extern/sqlite3
)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${PROJECT_SOURCE_DIR}/include
)

# # Thêm warning flags
# target_compile_options(${PROJECT_NAME} PRIVATE
#     -Wall
#     -Wextra
#     -Wpedantic
#     -Wconversion
#     -Wshadow
#     -Wnon-virtual-dtor
#     -Wold-style-cast
#     -Wno-unused-parameter
# )

# # Thêm clang-tidy
# set_target_properties(${PROJECT_NAME} PROPERTIES
#     CXX_CLANG_TIDY "clang-tidy;-checks=clang-analyzer-*,bugprone-*,performance-*,modernize-*,readability-*,cppcoreguidelines-init-variables,google-build-using-namespace,cppcoreguidelines-pro-type-member-init,cppcoreguidelines-slicing,-modernize-use-trailing-return-type"
# )

# Thêm file sqlite3.c vào project để build cùng
# set(SQLITE_SOURCE_FILE ${PROJECT_SOURCE_DIR}/extern/sqlite3/sqlite3.c)

# Link import library
# Tạo biến cho đường dẫn tuyệt đối tới .lib
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(SQLITE3_LIB "${CMAKE_SOURCE_DIR}/extern/sqlite3/sqlite3.lib")
  target_link_libraries(${PROJECT_NAME} PRIVATE "${SQLITE3_LIB}")
# elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
#   # Linux: Thêm sqlite3.c vào target để build cùng
#   target_sources(${PROJECT_NAME} PRIVATE ${SQLITE_SOURCE_FILE})

#   # Cài đặt cờ để bật thread-safe cho sqlite và liên kết thư viện pthreads
#   target_compile_definitions(${PROJECT_NAME} PRIVATE
#       SQLITE_THREADSAFE=1
#       SQLITE_ENABLE_FTS5=1
#   )
#   target_link_libraries(${PROJECT_NAME} PRIVATE pthread)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE sqlite3_wrapper)

target_precompile_headers(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include/pch.hpp)

# Liên kết OpenSSL
find_package(OpenSSL REQUIRED)

if (OpenSSL_FOUND)
    message(STATUS "OpenSSL found, version: ${OPENSSL_VERSION}")
    # Link to the OpenSSL libraries
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
else()
    message(FATAL_ERROR "OpenSSL not found!")
endif()

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

# Cấu hình thuộc tính target
set_target_properties(${PROJECT_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"    # Di chuyển các file sau khi build vào /bin
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  # Copy DLL vào output folder sau khi build
  add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/extern/sqlite3/sqlite3.dll
          $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()

]]#