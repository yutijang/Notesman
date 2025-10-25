get_filename_component(_VCPKG_TRIPLET_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

include("${_VCPKG_TRIPLET_DIR}/../x64-linux.cmake")

set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CRT_LINKAGE dynamic)
