#pragma once

#ifdef _WIN32
#    if !defined(UNICODE) && !defined(_UNICODE)
#        define UNICODE
#        define _UNICODE
#    endif

#    define WIN32_LEAN_AND_MEAN

#    include <windows.h>
#endif

#include <string>      // IWYU pragma: keep
#include <string_view> // IWYU pragma: keep
#include <stdexcept>   // IWYU pragma: keep
#include <optional>    // IWYU pragma: keep
#include <vector>      // IWYU pragma: keep
#include <cstdint>     // IWYU pragma: keep
#include <format>      // IWYU pragma: keep
#include <filesystem>  // IWYU pragma: keep
#include <memory>      // IWYU pragma: keep
// #include <sqlite3.h>
