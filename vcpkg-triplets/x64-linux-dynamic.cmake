# Kế thừa triplet gốc của vcpkg
include("${CMAKE_CURRENT_LIST_DIR}/../vcpkg/triplets/x64-linux.cmake")

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
