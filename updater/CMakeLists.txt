if (WIN32)
  add_executable(updater updater_windows.cpp)

  target_link_options(updater PRIVATE "/SUBSYSTEM:CONSOLE")

  target_link_libraries(updater PRIVATE miniz_static)

  set_target_properties(updater PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
  )
elseif(UNIX)
  add_executable(updater updater_linux.cpp)

  set_target_properties(updater PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
  )
endif()