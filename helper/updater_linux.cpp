#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: updater_linux <current-app> <new-app>\n";
        return 1;
    }

    std::ofstream f("/tmp/updater_run.log", std::ios::app);
    f << "argv0=" << argv[0] << "\n";
    f << "argv1=" << argv[1] << "\n";
    f << "argv2=" << argv[2] << "\n";
    f << "exists(argv1)=" << (access(argv[1], F_OK) == 0) << "\n";
    f << "exists(argv2)=" << (access(argv[2], F_OK) == 0) << "\n";

    const fs::path currentApp = argv[1];
    const fs::path newApp = argv[2];

    std::this_thread::sleep_for(
        std::chrono::milliseconds(300)); // NOLINT(readability-magic-numbers)

    ::chmod(newApp.c_str(), 0755);       // NOLINT(readability-magic-numbers)

    try {
        fs::rename(newApp, currentApp);
    } catch (const std::exception &e) {
        std::cerr << "rename failed: " << e.what() << "\n";
        return 2;
    }

    const std::string app = currentApp.string();
    char* const args[] = {const_cast<char*>(app.c_str()), const_cast<char*>("--update-done"),
                          nullptr};

    ::execv(app.c_str(), args);

    perror("execv failed");
    return 3;
}
