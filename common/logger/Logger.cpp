#include "common/logger/Logger.hpp"

#if defined(_WIN32)
#include <Windows.h>
#else
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <linux/limits.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <filesystem>
#include <memory>
#include <mutex>
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <system_error>
#include <vector>

namespace {

std::once_flag gOnce;

[[maybe_unused]] std::filesystem::path getExeDir() {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
#else
    char buf[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) {
        return std::filesystem::current_path();
    }
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

std::filesystem::path getLogDir() {
#if defined(_WIN32)
    return getExeDir() / "logs";
#else
    if (char const* xdg = std::getenv("XDG_STATE_HOME")) {
        return std::filesystem::path(xdg) / "notesman" / "logs";
    }

    if (char const* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".local/state/notesman/logs";
    }

    return std::filesystem::temp_directory_path() / "notesman/logs";
#endif
}

} // namespace

namespace Log::detail {

spdlog::logger*& loggerInstance() {
    static spdlog::logger* gLogger{};
    return gLogger;
}

} // namespace Log::detail

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Log::init(std::string const& loggerName, std::string const& fileName) {
    std::call_once(gOnce, [&] {
        std::vector<spdlog::sink_ptr> sinks;

        // ---- FILE SINK (BẮT BUỘC) ----
        auto logPath = getLogDir() / std::filesystem::path(fileName).filename();

        std::error_code ec;
        std::filesystem::create_directories(logPath.parent_path(), ec);
        if (ec) {
            // fallback
            logPath = std::filesystem::temp_directory_path() / "notesman_error.log";
        }

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
