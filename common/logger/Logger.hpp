#pragma once

#include <concepts>
#include <fmt/base.h>
#include <fmt/format.h>
#include <source_location>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Log {
    void init(std::string const& loggerName = "app",
              std::string const& fileName = "logs/error.log");

    struct SourceLocFmt {
            std::string_view fmt;
            std::source_location loc;

            template<typename T>
                requires std::convertible_to<T, std::string_view>
            constexpr SourceLocFmt(T const& s,
                                   std::source_location l = std::source_location::current())
                : fmt(s), loc(l) {}
    };

    namespace detail {
        inline spdlog::logger*& loggerInstance() {
            static spdlog::logger* gLogger{};
            return gLogger;
        }

        inline spdlog::logger& get() {
            if (loggerInstance() == nullptr) { throw std::logic_error("Logger not initialized"); }
            return *loggerInstance();
        }
    } // namespace detail

    template<typename... Args> inline void info(SourceLocFmt sf, Args&&... args) {
        detail::get().log(spdlog::source_loc{sf.loc.file_name(), static_cast<int>(sf.loc.line()),
                                             sf.loc.function_name()},
                          spdlog::level::info, fmt::runtime(sf.fmt), std::forward<Args>(args)...);
    }

    template<typename... Args> inline void warn(SourceLocFmt sf, Args&&... args) {
        detail::get().log(spdlog::source_loc{sf.loc.file_name(), static_cast<int>(sf.loc.line()),
                                             sf.loc.function_name()},
                          spdlog::level::warn, fmt::runtime(sf.fmt), std::forward<Args>(args)...);
    }

    template<typename... Args> inline void err(SourceLocFmt sf, Args&&... args) {
        detail::get().log(spdlog::source_loc{sf.loc.file_name(), static_cast<int>(sf.loc.line()),
                                             sf.loc.function_name()},
                          spdlog::level::err, fmt::runtime(sf.fmt), std::forward<Args>(args)...);
    }

    template<typename... Args> inline void fatal(SourceLocFmt sf, Args&&... args) {
        detail::get().log(spdlog::source_loc{sf.loc.file_name(), static_cast<int>(sf.loc.line()),
                                             sf.loc.function_name()},
                          spdlog::level::critical, fmt::runtime(sf.fmt),
                          std::forward<Args>(args)...);
    }
} // namespace Log
