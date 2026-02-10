#pragma once

#include <cstdint>

enum class ContentMode : std::uint8_t {
    HtmlFile, // HTML đơn lẻ
    Epub,     // HTML trong container
    Url       // Web page
};
