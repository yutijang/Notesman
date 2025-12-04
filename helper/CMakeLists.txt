add_executable(updater updater.cpp)

set_target_properties(updater PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
)
