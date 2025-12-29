set(LAUNCHER launcher)

add_executable(${LAUNCHER} WIN32
  launcher.cpp
  simple_log.cpp
)

target_include_directories(${LAUNCHER}
  PRIVATE
    "${PROJECT_SOURCE_DIR}/apps/notesman/helper"
    "${PROJECT_SOURCE_DIR}/apps/notesman"
)

target_compile_definitions(${LAUNCHER}
  PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(${LAUNCHER}
  PRIVATE
    kernel32
    user32
    shell32
    bcrypt
    shlwapi
)

# --- metadata ---------------------------------------
set(PROJECT_AUTHOR "yutijang")
set(PROJECT_WEBSITE "https://notesman-oauth.netlify.app")
set(PROJECT_NAME_ORI "Launcher")
set(PROJECT_DESCRIPTION "Notesman")
set(PROJECT_INTERNAL_NAME "Launcher.exe")
set(PROJECT_FILENAME "Notesman.exe")
# ----------------------------------------------------

set(LAUNCHER_GENERATED_DIR
    "${CMAKE_BINARY_DIR}/generated/launcher"
)
file(MAKE_DIRECTORY "${LAUNCHER_GENERATED_DIR}")

set(LAUNCHER_VERSION_RC
  "${LAUNCHER_GENERATED_DIR}/version.rc"
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/resources/version.rc.in"
  "${LAUNCHER_VERSION_RC}"
  @ONLY
)

set(LAUNCHER_ICON_RC
  "${LAUNCHER_GENERATED_DIR}/app_icon.rc"
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/resources/app_icon.rc.in"
  "${LAUNCHER_ICON_RC}"
  @ONLY
)

target_sources(${LAUNCHER}
  PRIVATE
    ${LAUNCHER_VERSION_RC}
    ${LAUNCHER_ICON_RC}
)

set_target_properties(${LAUNCHER} PROPERTIES
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  OUTPUT_NAME "Notesman"
)
