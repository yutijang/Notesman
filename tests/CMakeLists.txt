# Tìm Catch2 v3 được cài qua vcpkg
find_package(Catch2 CONFIG REQUIRED)

# Set general output dir
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin/tests")

# Tự động phát hiện TEST_CASE() của Catch2
include(CTest)
include(Catch)

add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(viewer_packer)