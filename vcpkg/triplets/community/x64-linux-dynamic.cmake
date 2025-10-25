set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Dùng clang thay vì gcc
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "/usr/share/cmake/clang-toolchain.cmake" CACHE STRING "")
