#pragma once

#include <string_view>
#include <source_location>

namespace Log {
    void init();
    void info(std::string_view fmt, std::source_location loc = std::source_location::current());
    void warn(std::string_view fmt, std::source_location loc = std::source_location::current());
    void err(std::string_view fmt, std::source_location loc = std::source_location::current());
    void fatal(std::string_view fmt, std::source_location loc = std::source_location::current());
} // namespace Log
