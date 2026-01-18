set(CMAKE_PCH_WARN_INVALID ON)

# --- metadata -----------------------------------------------
set(NOTESMAN_AUTHOR "yutijang")
set(NOTESMAN_WEBSITE "https://notesman-oauth.netlify.app")
set(NOTESMAN_NAME_ORI "Notesman")
set(NOTESMAN_DESCRIPTION "Notesman")
set(NOTESMAN_INTERNAL_NAME "NotesmanCore.dll")
set(NOTESMAN_FILENAME "NotesmanCore.dll")
# ------------------------------------------------------------

# --- OAuth config (Notesman only) ---------------------------
if(DEFINED ENV{GOOGLE_CLIENT_ID})
  set(CLIENT_ID     "$ENV{GOOGLE_CLIENT_ID}")
  set(CLIENT_SECRET "$ENV{GOOGLE_CLIENT_SECRET}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/oauth_private.cmake")
  include("${CMAKE_CURRENT_SOURCE_DIR}/oauth_private.cmake")
endif()

set(NOTESMAN_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/notesman")
file(MAKE_DIRECTORY ${NOTESMAN_GENERATED_DIR})

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/templates/google_oauth_config.hpp.in
    ${NOTESMAN_GENERATED_DIR}/google_oauth_config.hpp
    @ONLY
)
# ------------------------------------------------------------

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/templates/app_version.hpp.in
    ${NOTESMAN_GENERATED_DIR}/app_version.hpp
    @ONLY
)

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/templates/schema_version.hpp.in
    ${NOTESMAN_GENERATED_DIR}/schema_version.hpp
    @ONLY
)

# =========================================================
# Core library
# =========================================================
add_library(notes-core STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/repository/resource_repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/repository/text_content_repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/repository/file_repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/repository/tag_repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/repository/file_text_content_repository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/service/file_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/service/resource_service.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/settings/AppSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/db/database_checker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/helper.cpp
)

target_include_directories(notes-core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/db
    ${CMAKE_CURRENT_SOURCE_DIR}/core/model
    ${CMAKE_CURRENT_SOURCE_DIR}/core/repository
    ${CMAKE_CURRENT_SOURCE_DIR}/core/service
    ${CMAKE_CURRENT_SOURCE_DIR}/core/settings 
    ${CMAKE_CURRENT_SOURCE_DIR}/helper
    ${NOTESMAN_GENERATED_DIR} # db version
)

target_precompile_headers(notes-core
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/pch_stl.hpp
)

target_link_libraries(notes-core
  PUBLIC
    SQLite::SQLite3
    OpenSSL::SSL
    OpenSSL::Crypto
    common::logger
)

target_compile_definitions(notes-core
  PRIVATE
    $<$<PLATFORM_ID:Windows>:UNICODE>
    $<$<PLATFORM_ID:Windows>:_UNICODE>
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
)

# =========================================================
# Sources
# =========================================================
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/application/NotesAppCore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/application/AppController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/application/AppInitializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/application/workers/ResourceSearchWorker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/application/services/UpdateManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/application/services/DownloadManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/application/services/GoogleDriveService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/application/services/ResourceViewService.cpp
)

set(GUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/MainWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Widgets/BrowseTabWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Widgets/AddTabWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Widgets/SettingsTabWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Widgets/InfoCornerWidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Highlighter/cpphighlighter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Highlighter/CodeEditorLineHighlighter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/TagInput/TagInput.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/viewer/IResourceViewer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/viewer/TextViewer.cpp
)

set(HELPER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/FontLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/ResultsTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/ResourceTitleDelegate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/PlainTextEdit.cpp    
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/DialogUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/ResourceViewerDialog.cpp
)

if(UNIX)
  list(APPEND HELPER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/AppImageExtractor.cpp
  )
endif()

# =========================================================
# notes-app-lib (App + GUI)
# =========================================================
add_library(notes-app-lib-core STATIC
    ${APP_SOURCES}
    ${GUI_SOURCES}
    ${HELPER_SOURCES}
)

target_include_directories(notes-app-lib-core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/application
    ${CMAKE_CURRENT_SOURCE_DIR}/application/workers
    ${CMAKE_CURRENT_SOURCE_DIR}/application/services
    ${CMAKE_CURRENT_SOURCE_DIR}/gui
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Highlighter
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/TagInput
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/viewer
    ${CMAKE_CURRENT_SOURCE_DIR}/helper
    ${NOTESMAN_GENERATED_DIR} # OAuth config include dir
)

target_precompile_headers(notes-app-lib-core
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/pch_qt.hpp
)

target_link_libraries(notes-app-lib-core
  PUBLIC
    notes-core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
)

target_compile_definitions(notes-app-lib-core
  PRIVATE
    $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>
    $<$<CONFIG:Release>:QT_NO_DEBUG_WARNING_OUTPUT>
    $<$<CONFIG:Release>:QT_NO_DEBUG_FATAL_WARNINGS>
)

#===
add_library(notes-app-lib-qtkeychain STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/application/services/OAuthManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/free_port.cpp
)

target_include_directories(notes-app-lib-qtkeychain
  SYSTEM PRIVATE
    ${qtkeychain_SOURCE_DIR}/qtkeychain
    ${qtkeychain_BINARY_DIR}/qtkeychain
)

target_precompile_headers(notes-app-lib-qtkeychain
  REUSE_FROM notes-app-lib-core
)

target_link_libraries(notes-app-lib-qtkeychain
  PRIVATE
    notes-app-lib-core
    qt6keychain
)
#===

# =========================================================
# Executable (GUI entry point)
# =========================================================
add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc
)

target_precompile_headers(${PROJECT_NAME} REUSE_FROM notes-app-lib-core)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    notes-app-lib-core # Thứ tự QUAN TRỌNG
    notes-app-lib-qtkeychain
    $<$<TARGET_EXISTS:security_utils>:security_utils>
)

if(WIN32)
  set(GENERATED_RC_DIR
      "${CMAKE_BINARY_DIR}/generated/notesman"
  )  
  file(MAKE_DIRECTORY "${GENERATED_RC_DIR}")

  set(NOTESMAN_VERSION_RC
      "${GENERATED_RC_DIR}/version.rc"
  )

  configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/resources/version.rc.in
      ${NOTESMAN_VERSION_RC}
      @ONLY
  )

  set(NOTESMAN_ICON_RC
      "${GENERATED_RC_DIR}/icon.rc"
  )

  configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/resources/app_icon.rc.in
      ${NOTESMAN_ICON_RC}
      @ONLY
  )

  target_sources(${PROJECT_NAME}
    PRIVATE
      ${NOTESMAN_VERSION_RC}
      ${NOTESMAN_ICON_RC}
  )

  set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE $<IF:$<CONFIG:Release>,TRUE,FALSE>
    OUTPUT_NAME "NotesmanCore"
    SUFFIX ".dll"
  )    
endif()

target_compile_definitions(${PROJECT_NAME}
  PRIVATE
    $<$<CONFIG:Release>:QT_NO_DEBUG_OUTPUT>
    $<$<CONFIG:Release>:QT_NO_DEBUG_WARNING_OUTPUT>
    $<$<CONFIG:Release>:QT_NO_DEBUG_FATAL_WARNINGS>
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
