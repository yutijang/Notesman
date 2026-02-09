#pragma once

#include <cstdint>

enum class ViewerPackError : std::uint8_t {
    FileOpenFailed,
    FileTooSmall,
    InvalidMagic,
    UnsupportedVersion,
    HeaderSizeMismatch,
    CrcMismatch,
};
