#pragma once

#include <cstdint>

enum class ViewerPackError : std::uint8_t {
    FileOpenFailed,
    FileSizeMismatch,
    MagicMismatch,
    UnsupportedVersion,
    HeaderSizeMismatch,
    CrcMismatch,
};
