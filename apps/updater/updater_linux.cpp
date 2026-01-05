#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace fs = std::filesystem;

void logUpdater(const std::string &msg);

int main(int argc, char** argv) {
    logUpdater("Updater đã khởi chạy.");

    if (argc < 4) {
        logUpdater("LỖI: Thiếu đối số (argc < 4)");
        return 1;
    }

    std::error_code ec;
    const fs::path targetApp = fs::absolute(argv[1], ec);
    const fs::path newApp = fs::absolute(argv[2], ec);
    pid_t parentPid = std::stoi(argv[3]);

    logUpdater("Current App (đích): " + targetApp.string());
    logUpdater("New App (nguồn): " + newApp.string());
    logUpdater("Đang chờ Parent PID: " + std::to_string(parentPid));

    if (ec || !fs::exists(targetApp) || !fs::exists(newApp)) { return 2; }

    // NOLINTBEGIN (readability-magic-numbers)
    logUpdater("Đang chờ tiến trình cha (PID: " + std::to_string(parentPid) + ") thoát...");
    int attempts = 0;
    while (attempts < 50) { // Chờ tối đa 5 giây
        if (kill(parentPid, 0) == -1 && errno == ESRCH) {
            logUpdater("Tiến trình cha đã thoát thực sự.");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        attempts++;

        // Nếu sau 2 giây chưa thoát, gửi SIGTERM cho chắc chắn
        if (attempts == 20) {
            logUpdater("Cha quá lâu chưa thoát, gửi SIGTERM...");
            kill(parentPid, SIGTERM);
        }
    }

    // Nghỉ thêm 200ms để Kernel giải phóng file lock
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // NOLINTEND

    // std::this_thread::sleep_for(
    //     std::chrono::milliseconds(500)); // NOLINT(readability-magic-numbers)

    logUpdater("newApp: " + newApp.string());
    logUpdater("targetApp: " + targetApp.string());

    logUpdater("Đang thực hiện ghi đè file...");

    fs::rename(newApp, targetApp, ec);

    if (ec) {
        logUpdater("Rename thất bại, thử dùng Copy...");
        fs::copy_file(newApp, targetApp, fs::copy_options::overwrite_existing, ec);
    }

    if (ec) {
        logUpdater("LỖI CẬP NHẬT TRẦM TRỌNG: " + ec.message());
        return 4;
    }

    if (::chmod(targetApp.c_str(), 0755) != 0) { // NOLINT(readability-magic-numbers)
        logUpdater("LỖI chmod");
        return 5;                                // NOLINT(readability-magic-numbers)
    }

    // prepare args for execv
    // argv[0] = targetApp
    // argv[1] = --update-done
    // argv[2] = oldApp (currentApp)
    // argv[3] = updater path (selfPath)

    const std::string appPath = targetApp.string();

    const fs::path selfPath = fs::read_symlink("/proc/self/exe");
    const std::string updaterStr = selfPath.string();

    std::vector<std::string> argsStr{appPath, "--update-done", appPath, updaterStr};

    std::vector<char*> args;
    args.reserve(argsStr.size());
    for (auto &s : argsStr) {
        args.push_back(s.data()); // C++17+: writable buffer
    }
    args.push_back(nullptr);

    logUpdater("Đang khởi động lại ứng dụng...");

    ::execv(args[0], args.data());

    perror("execv failed");

    return 6; // NOLINT(readability-magic-numbers)
}

void logUpdater(const std::string &msg) {
    std::ofstream f("/tmp/notesman_debug.log", std::ios::app);
    if (f.is_open()) { f << "[UPDATER] " << msg << '\n'; }
}
