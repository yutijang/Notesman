#include <mutex>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <string>

#include "Logger.hpp"

namespace {
    std::once_flag gOnce;
} // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Log::init(const std::string &loggerName, const std::string &fileName) {
    std::call_once(gOnce, [&] {
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(fileName, false);
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

        auto logger = std::make_unique<spdlog::logger>(
            loggerName, spdlog::sinks_init_list{fileSink, consoleSink});

        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%s:%#] %v");
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::err);

        Log::detail::loggerInstance() = logger.release(); // intentional leak
    });
}
