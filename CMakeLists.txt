cmake_minimum_required(VERSION 3.31)
project(Notesman
    VERSION 1.0.73
    LANGUAGES C CXX
    DESCRIPTION "C++ notes manager for learn"
)

# Bật export compile commands cho clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Global metadata ----------------------------------------
set(PROJECT_AUTHOR "yutijang")
set(PROJECT_WEBSITE "https://github.com/yutijang/Notesman")
# ------------------------------------------------------------

# --- generate app_version.hpp for source code and clangd ----
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/templates/app_version.hpp.in
    ${GENERATED_DIR}/app_version.hpp
    @ONLY
)

if(WIN32)
    set(VERSION_RC_IN  "${CMAKE_SOURCE_DIR}/resources/version.rc.in")
    set(VERSION_RC_OUT "${CMAKE_BINARY_DIR}/generated/version.rc")
    configure_file(${VERSION_RC_IN} ${VERSION_RC_OUT} @ONLY)
    set(VERSION_RC ${VERSION_RC_OUT} CACHE INTERNAL "")

    set(ICON_RC_IN  "${CMAKE_SOURCE_DIR}/resources/app_icon.rc.in")
    set(ICON_RC_OUT "${CMAKE_BINARY_DIR}/generated/app_icon.rc")
    configure_file(${ICON_RC_IN} ${ICON_RC_OUT} @ONLY)
    set(APP_ICON_RC ${ICON_RC_OUT} CACHE INTERNAL "")
endif()
# ------------------------------------------------------------

# --- OAuth config -------------------------------------------
set(CLIENT_ID     "$ENV{GOOGLE_CLIENT_ID}")
set(CLIENT_SECRET "$ENV{GOOGLE_CLIENT_SECRET}")

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/templates/google_oauth_config.hpp.in
    ${GENERATED_DIR}/google_oauth_config.hpp
    @ONLY
)
# ------------------------------------------------------------

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Bật tối ưu hóa liên thủ tục (LTO) cho Release
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# Bỏ quét module/p1689 để tránh xung đột clang-scan-deps
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

find_package(OpenSSL REQUIRED)
find_package(Qt6 COMPONENTS Core Gui Widgets Network REQUIRED)

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

# ============================================================
# Qt setup
# ============================================================
# Bật Qt MOC/UIC/RCC cho toàn project
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release" AND DEFINED ENV{CI})
    # message(STATUS "Windows CI build detected → disabling precompiled headers")
    # set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
# endif()

# Thêm subdirectory app
add_subdirectory(extern/sqlite3)
add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
