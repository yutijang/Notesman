#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: updater_linux <current-app> <new-app>\n";
        return 1;
    }

    const fs::path currentApp = argv[1];
    const fs::path newApp = argv[2];

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    ::chmod(newApp.c_str(), 0755);

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
