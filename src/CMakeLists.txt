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
  PUBLIC
    sqlite3_wrapper
    OpenSSL::SSL
    OpenSSL::Crypto
)

# =========================================================
# Sources
# =========================================================
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/app/NotesAppCore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/AppController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/AppInitializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/UpdateManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/DownloadManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/workers/ResourceSearchWorker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/GoogleDriveService.cpp
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
)

set(HELPER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/FontLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/ResultsTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/PlainTextEdit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helper/DialogUtils.cpp
)

# =========================================================
# notes-app-lib (App + GUI)
# =========================================================
add_library(notes-app-lib-core STATIC
    ${APP_SOURCES}
    ${GUI_SOURCES}
    ${HELPER_SOURCES}
)

target_include_directories(notes-app-lib-core
  SYSTEM PRIVATE
    ${PROJECT_SOURCE_DIR}/extern/sqlite3
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/app/workers
    ${CMAKE_CURRENT_SOURCE_DIR}/gui
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Highlighter
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/TagInput
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/Widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/helper         
    ${GENERATED_DIR}
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
)

#===
add_library(notes-app-lib-qtkeychain STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/app/OAuthManager.cpp
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
    ${PROJECT_SOURCE_DIR}/resources/resources.qrc
)

target_precompile_headers(${PROJECT_NAME} REUSE_FROM notes-app-lib-core)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    notes-app-lib-core # Thứ tự QUAN TRỌNG
    notes-app-lib-qtkeychain
)

if(WIN32)
    target_sources(${PROJECT_NAME}
      PRIVATE
          ${VERSION_RC}
          ${APP_ICON_RC}
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
      WIN32_EXECUTABLE $<IF:$<CONFIG:Release>,TRUE,FALSE>
    )    
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
)

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
