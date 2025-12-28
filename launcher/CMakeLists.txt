set(LAUNCHER launcher)

add_executable(${LAUNCHER} WIN32
  launcher.cpp
  simple_log.cpp
)

target_include_directories(${LAUNCHER}
  PRIVATE
    "${PROJECT_SOURCE_DIR}/src/helper"
    "${PROJECT_SOURCE_DIR}/src"
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

target_sources(${LAUNCHER}
  PRIVATE
    ${VERSION_RC}
    ${APP_ICON_RC}
)

set_target_properties(${LAUNCHER} PROPERTIES
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
  OUTPUT_NAME "Notesman"
)
