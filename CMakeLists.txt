file(STRINGS "${CMAKE_SOURCE_DIR}/version.txt" VERSION_LINES)

foreach(line IN LISTS VERSION_LINES)
  string(STRIP "${line}" line)

  if(line MATCHES "^APP=(.+)$")
    set(PROJECT_VERSION_VALUE "${CMAKE_MATCH_1}")
  elseif(line MATCHES "^DB=([0-9]+)$")
    set(SCHEMA_VERSION_VALUE "${CMAKE_MATCH_1}")
  endif()
endforeach()

if(NOT DEFINED PROJECT_VERSION_VALUE)
  message(FATAL_ERROR "APP version not found in version.txt")
endif()

if(NOT DEFINED SCHEMA_VERSION_VALUE)
  message(FATAL_ERROR "DB version not found in version.txt")
endif()

cmake_minimum_required(VERSION 3.31)
project(Notesman
    VERSION ${PROJECT_VERSION_VALUE}
    LANGUAGES C CXX
    DESCRIPTION "C++ notes manager for learn"
)

# Bật export compile commands cho clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Bật tối ưu hóa liên thủ tục (LTO) cho Release
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# Bỏ quét module/p1689 để tránh xung đột clang-scan-deps
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

find_package(OpenSSL REQUIRED)
find_package(Qt6 COMPONENTS Core Gui Widgets Network Svg Concurrent REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)

find_package(PkgConfig)
pkg_check_modules(POPPLER_CPP REQUIRED IMPORTED_TARGET poppler-cpp)

find_package(md4c CONFIG REQUIRED)

if(WIN32)
    find_package(miniz CONFIG REQUIRED)
    find_package(unofficial-webview2 CONFIG REQUIRED)
endif()

# --- QtKeychain ---------------------------------------------
include(FetchContent)

FetchContent_Declare(
    qtkeychain
    GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
    GIT_TAG 0.15.0
)

set(BUILD_TRANSLATIONS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static QtKeychain" FORCE)
set(BUILD_WITH_QT6 ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(qtkeychain)

# --- Tắt warning cho mã nguồn qtkeychain ---
if (TARGET qt6keychain)
    target_compile_options(qt6keychain PRIVATE
        -Wno-ignored-attributes
        -Wno-attributes
    )
endif()
# ------------------------------------------------------------

# Set general output dir
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

# ============================================================
# Qt setup
# ============================================================
# Bật Qt MOC/UIC/RCC cho toàn project
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release" AND DEFINED ENV{CI})
    # message(STATUS "Windows CI build detected → disabling precompiled headers")
    # set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
# endif()

# Thêm subdirectory app
# add_subdirectory(extern/sqlite3)
# add_subdirectory(extern/miniz)

# notesman core
add_subdirectory(apps/notesman)
# updater
add_subdirectory(apps/updater)
# Logger
add_subdirectory(common/logger)

# launcher
if(WIN32)
    add_subdirectory(apps/launcher)
    add_subdirectory(common/security_utils)
endif()

if(UNIX AND NOT APPLE)
    add_subdirectory(apps/webkitgtk_viewer)
endif()

enable_testing()
add_subdirectory(tests)
