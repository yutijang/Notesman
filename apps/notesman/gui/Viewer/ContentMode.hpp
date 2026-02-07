#pragma once

#include <cstdint>

enum class ContentMode : std::uint8_t {
    htmlFile, // HTML đơn lẻ
    epub,     // HTML trong container
    url       // Web page
};
