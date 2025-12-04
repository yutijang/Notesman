// updater.cpp
// Build: cl /std:c++17 updater.cpp /link advapi32 shell32
// or with MSVC/CMake. This is Windows-only.

#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <cstdlib>

namespace fs = std::filesystem;

static void log(const std::string &s) {
    std::cerr << s << std::endl;
}

static int runCommandBlocking(const std::string &cmd) {
    // runs command via system() and returns exit code.
    // Using system() for simplicity (calls cmd.exe). Could use CreateProcess for more control.
    int rc = std::system(cmd.c_str());
    return rc;
}

static bool waitForFileBeFree(const fs::path &file, int timeoutSeconds) {
    // Try renaming the exe; if succeeds, rename back and return true.
    // This indicates no process locks it for writing. Retry until timeout.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
    const fs::path tmpName = file.string() + ".updating_tmp";
    while (std::chrono::steady_clock::now() < deadline) {
        std::error_code ec;
        // if file does not exist, it's free
        if (!fs::exists(file, ec)) { return true; }
        // try to rename
        fs::rename(file, tmpName, ec);
        if (!ec) {
            // rename back
            fs::rename(tmpName, file, ec);
            if (ec) { log("Warning: rename back failed: " + ec.message()); }
            return true;
        }
        // cannot rename -> probably locked; wait and retry
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return false;
}

static void copyRecursive(const fs::path &from, const fs::path &to) {
    std::error_code ec;
    if (!fs::exists(to)) { fs::create_directories(to, ec); }
    for (auto &p : fs::recursive_directory_iterator(from)) {
        const auto rel = fs::relative(p.path(), from, ec);
        if (ec) { throw std::runtime_error("relative failed: " + ec.message()); }
        const fs::path dest = to / rel;
        if (p.is_directory()) {
            fs::create_directories(dest, ec);
        } else if (p.is_regular_file()) {
            fs::create_directories(dest.parent_path(), ec);
            fs::copy_file(p.path(), dest, fs::copy_options::overwrite_existing, ec);
            if (ec) { throw std::runtime_error("copy_file failed: " + ec.message()); }
        }
    }
}

static void removeAllExcept(const fs::path &dir, const std::vector<std::string> &keepNames) {
    std::error_code ec;
    for (auto &p : fs::directory_iterator(dir, ec)) {
        if (ec) { throw std::runtime_error("directory_iterator failed: " + ec.message()); }
        const auto name = p.path().filename().string();
        bool keep = false;
        for (auto &k : keepNames) {
            if (_stricmp(k.c_str(), name.c_str()) == 0) {
                keep = true;
                break;
            }
        }
        if (keep) { continue; }
        fs::remove_all(p.path(), ec);
        if (ec) { throw std::runtime_error("remove_all failed: " + ec.message()); }
    }
}

int main(int argc, char** argv) {
    if (argc < 4) {
        log("Usage: updater.exe <targetDir> <zipPath> <appExeName>");
        return 1;
    }

    const fs::path targetDir = fs::u8path(argv[1]);
    const fs::path zipPath = fs::u8path(argv[2]);
    const std::string appExeName = argv[3];

    log("Updater started");
    log("Target dir: " + targetDir.string());
    log("Zip path: " + zipPath.string());
    log("App exe name: " + appExeName);

    try {
        if (!fs::exists(zipPath)) {
            log("Error: zip not found");
            return 2;
        }
        if (!fs::exists(targetDir)) {
            log("Error: target dir not found");
            return 3;
        }

        const fs::path appExe = targetDir / appExeName;

        // 1) Wait for exe to be free (app terminated)
        log("Waiting for application to exit...");
        bool free = waitForFileBeFree(appExe, 30); // wait up to 30s
        if (!free) {
            log("Error: timeout waiting for application to exit or file lock persists.");
            return 4;
        }

        // 2) Create temp extraction dir
        fs::path tempExtract = targetDir / "_update_tmp";
        if (fs::exists(tempExtract)) { fs::remove_all(tempExtract); }
        fs::create_directories(tempExtract);

        // 3) Extract zip using PowerShell Expand-Archive for reliability
        // Build PowerShell command: Expand-Archive -Force -DestinationPath <tempExtract> -Path
        // <zipPath>
        std::string psCmd =
            "powershell -NoProfile -NonInteractive -Command \"Expand-Archive -Force -Path '";
        psCmd += zipPath.string();
        psCmd += "' -DestinationPath '";
        psCmd += tempExtract.string();
        psCmd += "'\"";

        log("Extracting update using PowerShell...");
        int rc = runCommandBlocking(psCmd);
        if (rc != 0) {
            log("Error: extraction failed, rc=" + std::to_string(rc));
            // cleanup
            try {
                fs::remove_all(tempExtract);
            } catch (...) {}
            return 5;
        }

        // 4) Backup current dir (rename) to .bak
        fs::path backupDir = targetDir;
        backupDir += ".bak";
        if (fs::exists(backupDir)) { fs::remove_all(backupDir); }
        // Move targetDir content into backup folder (we'll remove except keep later)
        // To simplify, we rename the whole directory to backup, then recreate targetDir empty.
        // But we cannot rename targetDir itself because updater.exe runs inside it.
        // So we will individually move content.
        // Instead, create backupDir and move files.
        fs::create_directories(backupDir);
        for (auto &p : fs::directory_iterator(targetDir)) {
            const auto name = p.path().filename().string();
            if (name == "_update_tmp" || name == "updater.exe") {
                continue; // skip temp and updater
            }
            fs::rename(p.path(), backupDir / name);
        }

        // 5) Copy extracted files into targetDir
        log("Copying new files...");
        copyRecursive(tempExtract, targetDir);

        // 6) Keep user files from backup (data.db, config.ini)
        std::vector<std::string> keep = {"data.db", "config.ini", "updater.exe"};
        for (auto &k : keep) {
            fs::path src = backupDir / k;
            if (fs::exists(src)) {
                fs::path dest = targetDir / k;
                // overwrite
                std::error_code ec;
                fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
                if (ec) {
                    log(std::string("Warning: could not restore ") + k + ": " + ec.message());
                }
            }
        }

        // 7) Cleanup: remove tempExtract, zip, backup
        try {
            fs::remove_all(tempExtract);
        } catch (...) {}
        try {
            fs::remove(zipPath);
        } catch (...) {}
        try {
            fs::remove_all(backupDir);
        } catch (...) {}

        // 8) Start the application
        std::string exeToStart = (targetDir / appExeName).string();
        log("Starting application: " + exeToStart);

        STARTUPINFOA si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);
        if (!CreateProcessA(exeToStart.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr,
                            targetDir.string().c_str(), &si, &pi)) {
            log("Error: CreateProcess failed, code=" + std::to_string(GetLastError()));
            return 7;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        log("Update finished successfully.");
        return 0;
    } catch (const std::exception &ex) {
        log(std::string("Exception: ") + ex.what());
        return 99;
    }
}
