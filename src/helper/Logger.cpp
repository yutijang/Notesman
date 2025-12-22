#include "Logger.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <mutex>

namespace {
    std::once_flag gOnce;
} // namespace

void Log::init() {
    std::call_once(gOnce, [] {
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/error.log", true);
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        auto logger = std::make_unique<spdlog::logger>(
            "notesman", spdlog::sinks_init_list{fileSink, consoleSink});

        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::err);

        Log::detail::loggerInstance() = logger.release(); // intentional leak
    });
}
