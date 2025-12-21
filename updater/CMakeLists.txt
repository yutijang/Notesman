if(WIN32)
  set(UPDATER_SRC updater_windows.cpp)
elseif(UNIX)
  set(UPDATER_SRC updater_linux.cpp)
endif()

set(UPDATER_PROJECT_NAME updater)
add_executable(${UPDATER_PROJECT_NAME} ${UPDATER_SRC})

if (WIN32)
  target_link_options(${UPDATER_PROJECT_NAME} PRIVATE "/SUBSYSTEM:CONSOLE")
  target_link_libraries(${UPDATER_PROJECT_NAME}
    PRIVATE
      miniz_static
      logger
  )
endif()

if(TARGET ${UPDATER_PROJECT_NAME})
  set_target_properties(${UPDATER_PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
  )
endif()