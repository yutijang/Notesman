#include <filesystem>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/stat.h>

#include "Logger.hpp"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    Log::init("updater");

    if (argc < 3) { return 1; }

    std::error_code ec;
    const fs::path currentApp = fs::weakly_canonical(argv[1], ec);
    const fs::path newApp = fs::weakly_canonical(argv[2], ec);
    if (ec || !fs::exists(currentApp) || !fs::exists(newApp)) { return 2; }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(500)); // NOLINT(readability-magic-numbers)

    const fs::path targetApp = currentApp.parent_path() / newApp.filename();

    bool copySuccess = fs::copy_file(newApp, targetApp, fs::copy_options::overwrite_existing, ec);
    if (!copySuccess) { return 4; }

    if (::chmod(targetApp.c_str(), 0755) != 0) { return 5; } // NOLINT(readability-magic-numbers)

    // prepare args for execv
    // argv[0] = targetApp
    // argv[1] = --update-done
    // argv[2] = oldApp (currentApp)
    // argv[3] = updater path (selfPath)

    const std::string app = targetApp.string();
    const std::string oldAppStr = currentApp.string();

    const fs::path selfPath = fs::read_symlink("/proc/self/exe");
    const std::string updaterStr = selfPath.string();

    std::vector<std::string> argsStr{app, "--update-done", oldAppStr, updaterStr};

    std::vector<char*> args;
    args.reserve(argsStr.size());
    for (auto &s : argsStr) {
        args.push_back(s.data()); // C++17+: writable buffer
    }
    args.push_back(nullptr);

    ::execv(args[0], args.data());

    perror("execv failed");

    return 6; // NOLINT(readability-magic-numbers)
}
