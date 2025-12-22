#include "Logger.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
// #include <spdlog/sinks/rotating_file_sink.h>

#include <memory>
#include <mutex>
#include <source_location>

namespace {
    spdlog::logger* gLogger{};
    std::once_flag gOnce;

    spdlog::logger &get() {
        if (gLogger == nullptr) { throw std::logic_error("Logger not initialized"); }
        return *gLogger;
    }
} // namespace

void Log::init() {
    std::call_once(gOnce, [] {
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/error.log", true);

        // auto fileSink =
        //     std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/error.log",
        //                                                            5 * 1024 * 1024, // 5MB
        //                                                            3                // giữ 3 file
        //                                                            cũ
        //     );

        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        auto logger = std::make_unique<spdlog::logger>(
            "notesman", spdlog::sinks_init_list{fileSink, consoleSink});

        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");

        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::err);

        gLogger = logger.release(); // intentional leak
    });
}

static void logImpl(spdlog::level::level_enum lvl, std::string_view fmt, std::source_location loc) {
    get().log(
        spdlog::source_loc{loc.file_name(), static_cast<int>(loc.line()), loc.function_name()}, lvl,
        fmt);
}

void Log::info(std::string_view fmt, std::source_location loc) {
    logImpl(spdlog::level::info, fmt, loc);
}

void Log::warn(std::string_view fmt, std::source_location loc) {
    logImpl(spdlog::level::warn, fmt, loc);
}

void Log::err(std::string_view fmt, std::source_location loc) {
    logImpl(spdlog::level::err, fmt, loc);
}

void Log::fatal(std::string_view fmt, std::source_location loc) {
    logImpl(spdlog::level::critical, fmt, loc);
}
