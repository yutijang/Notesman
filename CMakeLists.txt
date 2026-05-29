cmake_minimum_required(VERSION 3.31)

# Modules
include(FetchContent)
# -----------------------------------------------------------

# Read project version
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
# -----------------------------------------------------------

# Project
project(Notesman
    VERSION ${PROJECT_VERSION_VALUE}
    LANGUAGES C CXX
    DESCRIPTION "C++ notes manager for learn"
)
# -----------------------------------------------------------

# Global build settings
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

# ============================================================
# Qt setup
# ============================================================
# Bật Qt MOC/UIC/RCC cho toàn project
# set(CMAKE_AUTOMOC ON)
# set(CMAKE_AUTORCC ON)

# Set general output dir
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
# -----------------------------------------------------------

# --- Add global flags --------------------------------------
add_library(project_warnings INTERFACE)

target_compile_options(project_warnings INTERFACE

    # Clang/GCC-style warnings (match clang, GCC, clang-cl)
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
        -Werror
        -Wall
        -Wextra
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Wcast-align
        -Wnull-dereference
        -Wdouble-promotion
        -Wimplicit-fallthrough
        -Wundef
        -Wswitch-enum
    >

    # clang-cl (expanding)
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<BOOL:${MSVC}>>:
        /WX
        /W4
        /EHsc
        /permissive-
        /utf-8
        /Zc:__cplusplus

        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-switch-default
        -Wno-nrvo

        -Wno-padded

        -Wno-exit-time-destructors
        -Wno-unsafe-buffer-usage
        -Wno-global-constructors
    >
    # xử lý "_autogen", cần xem xét
    # -Wno-unsafe-buffer-usage, -Wno-global-constructors

    # MSVC cl.exe only
    $<$<CXX_COMPILER_ID:MSVC>:
        /WX
        /W4
        /EHsc
        /permissive-
        /utf-8
        /Zc:__cplusplus
        /Zc:preprocessor
    >

    $<$<NOT:$<BOOL:${MSVC}>>:
        --target=x86_64-pc-linux-gnu
    >

    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<BOOL:${MSVC}>>:
        --target=x86_64-pc-windows-msvc
    >
)
# ------------------------------------------------------------
# --- Add sanitizers debug -----------------------------------
add_library(project_sanitizers INTERFACE)

# Linux: Clang (non-MSVC) hoặc GCC
set(IS_LINUX_CLANG_OR_GCC
    $<AND:$<CONFIG:Debug>,$<NOT:$<BOOL:${MSVC}>>,$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>>
)

# Windows: clang-cl (Clang + MSVC frontend)
set(IS_CLANG_CL
    $<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:Clang>,$<BOOL:${MSVC}>>
)

target_compile_options(project_sanitizers INTERFACE
    $<${IS_LINUX_CLANG_OR_GCC}:-fsanitize=address;-fno-omit-frame-pointer>
    $<${IS_CLANG_CL}:/fsanitize=address;/Zi>
)

target_link_options(project_sanitizers INTERFACE
    $<${IS_LINUX_CLANG_OR_GCC}:-fsanitize=address>
    # clang-cl: không cần link flag, runtime tự link qua compile flag
)
# ------------------------------------------------------------

# Dependencies
find_package(OpenSSL REQUIRED)
find_package(Qt6 COMPONENTS Core Gui Widgets Network Svg Concurrent REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)

# với poppler trên Windows + vcpkg, pkg-config là bắt buộc, không có alternative qua CMake config target.
find_package(PkgConfig REQUIRED)
pkg_check_modules(POPPLER_CPP REQUIRED IMPORTED_TARGET poppler-cpp)

find_package(md4c CONFIG REQUIRED)

if(WIN32)
    find_package(miniz CONFIG REQUIRED)
    find_package(unofficial-webview2 CONFIG REQUIRED)
endif()

find_package(boost_url CONFIG REQUIRED)
find_package(libzip CONFIG REQUIRED)
# ------------------------------------------------------------

# Third-party build
# --- QtKeychain ---------------------------------------------
FetchContent_Declare(
    qtkeychain
    GIT_REPOSITORY https://github.com/frankosterfeld/qtkeychain.git
    GIT_TAG aa6da344e1a20b9194e12bace3665caeea6b6304
    GIT_SHALLOW TRUE
)

set(BUILD_TRANSLATIONS OFF CACHE BOOL "" FORCE)
set(BUILD_TEST_APPLICATION OFF CACHE BOOL "" FORCE)
set(BUILD_WITH_QT6 ON CACHE BOOL "" FORCE) #important

# Save global state
set(PROJECT_BUILD_TESTING ${BUILD_TESTING})
set(PROJECT_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})

# Disable only for dependency
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static QtKeychain" FORCE)

FetchContent_MakeAvailable(qtkeychain)

# Restore global state
set(BUILD_TESTING ${PROJECT_BUILD_TESTING} CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS ${PROJECT_BUILD_SHARED_LIBS} CACHE BOOL "" FORCE)

# --- Tắt warning cho mã nguồn qtkeychain ---
if (TARGET qt6keychain)
    target_compile_options(qt6keychain PRIVATE
        -Wno-ignored-attributes
        -Wno-attributes
        -Wno-switch
    )
endif()
# ------------------------------------------------------------

# if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release" AND DEFINED ENV{CI})
    # message(STATUS "Windows CI build detected → disabling precompiled headers")
    # set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
# endif()

# Subprojects
# Thêm subdirectory app
# add_subdirectory(extern/sqlite3)
# add_subdirectory(extern/miniz)

# notesman core
add_subdirectory(apps/notesman)
# updater
add_subdirectory(apps/updater)
# Logger
add_subdirectory(common/logger)
# viewer_pack
add_subdirectory(common/viewer_pack)

# launcher
if(WIN32)
    add_subdirectory(apps/launcher)
    add_subdirectory(common/security_utils)
endif()

if(UNIX AND NOT APPLE)
    add_subdirectory(apps/webkitgtk_viewer)
endif()
# ------------------------------------------------------------

# Tests
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
# ------------------------------------------------------------

# Tools
if (EXISTS "${CMAKE_SOURCE_DIR}/tools/CMakeLists.txt")
    add_subdirectory(tools)
endif()
# ------------------------------------------------------------