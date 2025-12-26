#pragma once

#ifdef _WIN32
    #include <winsock2.h> // winsock2.h được sử dụng trong free_port.cpp
                          // cần khai báo nó trước windows.h để ngăn windows.h include winsock.h
    #include <windows.h>
#endif

#include <string>         // IWYU pragma: keep
#include <string_view>    // IWYU pragma: keep
#include <stdexcept>      // IWYU pragma: keep
#include <optional>       // IWYU pragma: keep
#include <vector>         // IWYU pragma: keep
#include <cstdint>        // IWYU pragma: keep
#include <format>         // IWYU pragma: keep
#include <filesystem>     // IWYU pragma: keep
#include <memory>         // IWYU pragma: keep
#include <utility>        // IWYU pragma: keep
#include <sqlite3.h>      // IWYU pragma: keep

#include "model.hpp"      // IWYU pragma: keep
#include "sqldb_raii.hpp" // IWYU pragma: keep
