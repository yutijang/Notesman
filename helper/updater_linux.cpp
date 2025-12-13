#include <filesystem>
#include <string>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 3) { return 1; }

    const fs::path currentApp = fs::weakly_canonical(argv[1]);
    const fs::path newApp = fs::weakly_canonical(argv[2]);

    std::this_thread::sleep_for(
        std::chrono::milliseconds(500)); // NOLINT(readability-magic-numbers)

    if (!fs::exists(newApp)) { return 2; }

    const fs::path targetApp = currentApp.parent_path() / newApp.filename();

    try {
        fs::copy_file(newApp, targetApp, fs::copy_options::overwrite_existing);
    } catch (...) { return 3; }

    if (::chmod(targetApp.c_str(), 0755) != 0) { return 4; } // NOLINT(readability-magic-numbers)

    // prepare args for execv
    // argv[0] = targetApp
    // argv[1] = --update-done
    // argv[2] = oldApp (currentApp)
    // argv[3] = updater path (selfPath)

    const std::string app = targetApp.string();
    const std::string oldAppStr = currentApp.string();

    const fs::path selfPath = fs::read_symlink("/proc/self/exe");
    const std::string updaterStr = selfPath.string();

    char* const args[] = {const_cast<char*>(app.c_str()), const_cast<char*>("--update-done"),
                          const_cast<char*>(oldAppStr.c_str()),
                          const_cast<char*>(updaterStr.c_str()), nullptr};

    ::execv(app.c_str(), args);

    perror("execv failed");

    return 5; // NOLINT(readability-magic-numbers)
}
