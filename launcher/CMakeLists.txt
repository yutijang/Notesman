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

target_compile_options(${LAUNCHER}
  PRIVATE
    $<$<CONFIG:Release>:/MT>
    $<$<CONFIG:Debug>:/MTd>
)

if(MSVC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")  # clang-cl
  target_link_options(${LAUNCHER}
    PRIVATE
      /NODEFAULTLIB:MSVCRT
  )
endif()

target_sources(${LAUNCHER}
  PRIVATE
    ${VERSION_RC}
    ${APP_ICON_RC}
)

set_target_properties(${LAUNCHER} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
  OUTPUT_NAME "Notesman"
)
