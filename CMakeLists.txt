cmake_minimum_required(VERSION 3.31)
project(Notesman
  VERSION 1.0.0
  LANGUAGES C CXX
  DESCRIPTION "C++ notes manager for learn"
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Bật export compile commands cho clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Bật tối ưu hóa liên thủ tục (LTO) cho Release
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

find_package(OpenSSL REQUIRED)
find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

# Thêm subdirectory app
add_subdirectory(extern/sqlite3)
add_subdirectory(src)

enable_testing()
add_subdirectory(tests)