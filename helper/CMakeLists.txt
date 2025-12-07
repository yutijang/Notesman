add_executable(updater updater.cpp)

# target_link_options(updater PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")

target_link_libraries(updater PRIVATE miniz::miniz)

set_target_properties(updater PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
)
