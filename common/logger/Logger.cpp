#if defined(_WIN32)
    #include <windows.h>
#else
    #include <unistd.h>
    #include <limits.h>
#endif

#include <filesystem>
#include <mutex>
#include <memory>
#include <string>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "Logger.hpp"

namespace {
    std::once_flag gOnce;

    std::filesystem::path getExeDir() {
#if defined(_WIN32)
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        return std::filesystem::path(buf).parent_path();
#else
        char buf[PATH_MAX];
        ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len <= 0) { return std::filesystem::current_path(); }
        buf[len] = '\0';
        return std::filesystem::path(buf).parent_path();
#endif
    }

#ifdef _DEBUG
    bool hasConsole() {
    #if defined(_WIN32)
        DWORD mode{};
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        return h != INVALID_HANDLE_VALUE && h != nullptr && (GetConsoleMode(h, &mode) != 0);
    #else
        return ::isatty(STDOUT_FILENO);
    #endif
    }
#endif
} // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Log::init(const std::string &loggerName, const std::string &fileName) {
    std::call_once(gOnce, [&] {
        std::vector<spdlog::sink_ptr> sinks;

        // ---- FILE SINK (BẮT BUỘC) ----
        auto logPath = getExeDir() / fileName;
        std::filesystem::create_directories(logPath.parent_path());

        auto fileSink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), false);

        sinks.push_back(fileSink);

        // ---- CONSOLE SINK (CHỈ KHI CÓ CONSOLE) ----
#ifdef _DEBUG
        if (hasConsole()) {
            sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        }
#endif

        auto logger = std::make_shared<spdlog::logger>(loggerName, sinks.begin(), sinks.end());

        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [%s:%#] %v");

        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::err);

        spdlog::set_default_logger(logger);
        Log::detail::loggerInstance() = logger.get();
    });
}
