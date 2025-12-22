#pragma once

#include <string_view>
#include <source_location>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/format.h>
#include <stdexcept>

namespace Log {
    void init();

    namespace detail {
        inline spdlog::logger*&loggerInstance() {
            static spdlog::logger* gLogger{};
            return gLogger;
        }

        inline spdlog::logger &get() {
            if (loggerInstance() == nullptr) { throw std::logic_error("Logger not initialized"); }
            return *loggerInstance();
        }

        template<typename... Args>
        inline void logImpl(spdlog::level::level_enum lvl, std::string_view fmtStr,
                            Args &&... args) {
            get().log(lvl,
                      fmt::vformat(fmtStr, fmt::make_format_args(std::forward<Args>(args)...)));
        }

        template<typename... Args>
        inline void logImpl(spdlog::level::level_enum lvl, std::source_location loc,
                            std::string_view fmtStr, Args &&... args) {
            get().log(spdlog::source_loc{loc.file_name(), static_cast<int>(loc.line()),
                                         loc.function_name()},
                      lvl,
                      fmt::vformat(fmtStr, fmt::make_format_args(std::forward<Args>(args)...)));
        }
    } // namespace detail

    template<typename... Args> inline void info(Args &&... args) {
        detail::logImpl(spdlog::level::info, std::forward<Args>(args)...);
    }

    template<typename... Args> inline void warn(Args &&... args) {
        detail::logImpl(spdlog::level::warn, std::forward<Args>(args)...);
    }

    template<typename... Args> inline void err(Args &&... args) {
        detail::logImpl(spdlog::level::err, std::forward<Args>(args)...);
    }

    template<typename... Args> inline void fatal(Args &&... args) {
        detail::logImpl(spdlog::level::critical, std::forward<Args>(args)...);
    }
} // namespace Log
