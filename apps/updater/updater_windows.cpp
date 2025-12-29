#include <windows.h>
#include <unordered_set>
#include <string>
#include <filesystem>
#include <system_error>
#include <miniz.h>

#include "Logger.hpp"

namespace fs = std::filesystem;

namespace {
    std::string wstringToUtf8(const std::wstring &ws) {
        if (ws.empty()) { return {}; }
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
                                             nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<std::string::size_type>(sizeNeeded), 0);
        WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), result.data(),
                            sizeNeeded, nullptr, nullptr);
        return result;
    }

    // NOLINTNEXTLINE (bugprone-easily-swappable-parameters)
    void clearFolder(const std::wstring &targetFolder, const std::wstring &resDirName) {
        std::unordered_set<std::wstring> keepFiles{L"temp_update", L"data.db", L"config.ini",
                                                   L"logs"};

        if (!resDirName.empty() && resDirName != L"NULL_OR_ROOT") { keepFiles.insert(resDirName); }

        fs::path dirPath(targetFolder);
        std::error_code ec;
        if (!fs::exists(dirPath, ec) || !fs::is_directory(dirPath, ec)) {
            Log::err("Folder invalid: {} - error: {}", wstringToUtf8(targetFolder), ec.message());

            return;
        }

        for (const auto &entry : fs::directory_iterator(dirPath, ec)) {
            if (ec) {
                Log::err("Error: {}", ec.message());

                ec.clear();
                continue;
            }

            const auto &path = entry.path();
            const auto name = path.filename().wstring();

            if (keepFiles.contains(name)) { continue; }

            if (entry.is_directory(ec)) {
                fs::remove_all(path, ec);
                if (ec) { Log::err("Error remove all: {}", ec.message()); }
            } else {
                fs::remove(path, ec);
                if (ec) { Log::err("Error remove: {}", ec.message()); }
            }
            ec.clear();
        }
    }

    bool unzipToFolder(const std::wstring &zipPath, const std::wstring &folderInZip,
                       const fs::path &destFolder, bool overwrite) {
        if (!fs::exists(zipPath)) { return false; }
        fs::create_directories(destFolder);

        std::string zipPathUtf8 = wstringToUtf8(zipPath);
        mz_zip_archive zip{};
        mz_zip_zero_struct(&zip);
        if (mz_zip_reader_init_file(&zip, zipPathUtf8.c_str(), 0) == 0) { return false; }

        std::string folderUtf8;
        if (!folderInZip.empty()) {
            folderUtf8 = wstringToUtf8(folderInZip);
            if (folderUtf8.back() != '/') { folderUtf8 += '/'; }
        }

        const mz_uint num = mz_zip_reader_get_num_files(&zip);
        for (mz_uint i = 0; i < num; ++i) {
            mz_zip_archive_file_stat st;
            if (mz_zip_reader_file_stat(&zip, i, &st) == 0) {
                mz_zip_reader_end(&zip);
                return false;
            }

            std::string filename = st.m_filename;

            if (!folderUtf8.empty()) {
                if (!filename.starts_with(folderUtf8)) { continue; }
                filename = filename.substr(folderUtf8.size());
                if (filename.empty()) { continue; }
            }

            fs::path outPath = destFolder / filename;
            if (st.m_is_directory != 0) {
                fs::create_directories(outPath);
            } else {
                fs::create_directories(outPath.parent_path());
                if (overwrite || !fs::exists(outPath)) {
                    if (mz_zip_reader_extract_to_file(&zip, i, outPath.string().c_str(), 0) == 0) {
                        mz_zip_reader_end(&zip);
                        return false;
                    }
                }
            }
        }

        mz_zip_reader_end(&zip);
        return true;
    }

    // NOLINTNEXTLINE
    bool copyRecursive(const fs::path &from, const fs::path &to) {
        if (!fs::exists(from)) {
            Log::err("copyRecursive failed: source not found: {}", wstringToUtf8(from));
            return false;
        }

        std::error_code ec;

        fs::create_directories(to, ec);
        if (ec) {
            Log::err("copyRecursive failed: cannot create target directory: {} : {}",
                     wstringToUtf8(to), ec.message());
            return false;
        }

        fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

        if (ec) {
            Log::err("copyRecursive failed: {} (code {}) while copying from {} to {}", ec.message(),
                     ec.value(), wstringToUtf8(from), wstringToUtf8(to));
            return false;
        }

        return true;
    }

    bool waitForProcessExit(DWORD pid) {
        HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (h == nullptr) { return false; }

        DWORD result = WaitForSingleObject(h, INFINITE);
        CloseHandle(h);

        return result == WAIT_OBJECT_0;
    }

    DWORD getCurrentProcessId() {
        return ::GetCurrentProcessId();
    }

    void handleStage1(wchar_t* argv[]) {
        // arguments received from main app
        // argv[1] = --stage1
        // argv[2] = app PID
        // argv[3] = app dir
        // argv[4] = zip path
        // argv[5] = resource dir name

        const auto appPID = std::stoul(argv[2]);
        waitForProcessExit(appPID);

        const std::wstring appDir(argv[3]);
        const std::wstring tempDir = appDir + L"\\temp_update";
        const std::wstring zipPath(argv[4]);
        bool isUnzip = unzipToFolder(zipPath, L"Notesman-x64", tempDir, false);
        if (!isUnzip) {
            Log::err("Error unzip assets into temp_update folder");
            return;
        }

        // prepare arguments send to stage 2
        // argv[1] = --stage2
        // argv[2] = PID stage1 (current process: updater.exxe)
        // argv[3] = app dir
        // argv[4] = zip path
        // argv[5] = resource dir name

        const std::wstring exePath = tempDir + L"\\updater.exe";
        const std::wstring entryForStage2{L"--stage2"};
        const std::wstring currentPID = std::to_wstring(getCurrentProcessId());
        const std::wstring resDirName(argv[5]);

        const std::wstring cmdLine = L"\"" + exePath + L"\" "    // "C:\...\temp_update\updater.exe"
                                   + entryForStage2 + L" "       // --stage2
                                   + currentPID + L" "           // 1234
                                   + L"\"" + appDir + L"\" "     // "C:\Apps\Notesman"
                                   + L"\"" + zipPath + L"\" "    // "C:\Apps\update.zip"
                                   + L"\"" + resDirName + L"\""; // resources or NULL_OR_ROOT

        // CreateProcessW needs mutable buffer for cmdline
        std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back(0);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi{};

        BOOL ok = CreateProcessW(exePath.c_str(), // lpApplicationName
                                 cmdBuf.data(),   // lpCommandLine (mutable)
                                 nullptr, nullptr, FALSE, 0, nullptr,
                                 tempDir.c_str(), // lpCurrentDirectory (wide!)
                                 &si, &pi);

        if (ok == 0) {
            Log::err("CreateProcessW failed from stage1, error: {}", GetLastError());
        } else {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    void handleStage2(wchar_t* argv[]) {
        // arguments received from stage 1
        // argv[1] = --stage2
        // argv[2] = PID stage1 (current process: updater.exe)
        // argv[3] = app dir
        // argv[4] = zip path
        // argv[5] = resource dir name

        const auto stage1PID = std::stoul(argv[2]);
        waitForProcessExit(stage1PID);

        const std::wstring appDir(argv[3]);

        // delete all file/folder in app dir
        // except temp_update, data.db, config.ini, resources dir if exist
        const std::wstring resDir(argv[5]);
        clearFolder(appDir, resDir);

        const std::wstring tempDir = appDir + L"\\temp_update";
        bool isCopied = copyRecursive(tempDir, appDir);
        if (!isCopied) {
            Log::err("Error copy assets from: {} to: {}", wstringToUtf8(tempDir),
                     wstringToUtf8(appDir));
            return;
        }

        // prepare arguments send to main app
        // argv[1] = --update-done
        // argv[2] = PID stage2 (current process: updater.exxe)
        // argv[3] = temp_update dir path for delete
        // argv[4] = zip path

        const std::wstring entryForUpdateDone{L"--update-done"};
        const std::wstring currentPID = std::to_wstring(getCurrentProcessId());
        const std::wstring zipPath(argv[4]);

        const std::wstring exePath = appDir + L"\\Notesman.exe";

        const std::wstring cmdLine = L"\"" + exePath + L"\" "  // "C:\Apps\Notesman.exe"
                                   + entryForUpdateDone + L" " // --update-done
                                   + currentPID + L" "         // 5678
                                   + L"\"" + tempDir + L"\" "  // "C:\Apps\temp_update"
                                   + L"\"" + zipPath + L"\"";  // "C:\Apps\update.zip"

        std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back(0);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOWNORMAL;

        PROCESS_INFORMATION pi{};

        BOOL ok = CreateProcessW(exePath.c_str(), // lpApplicationName
                                 cmdBuf.data(),   // lpCommandLine (mutable)
                                 nullptr, nullptr, FALSE, 0, nullptr,
                                 appDir.c_str(),  // lpCurrentDirectory (wide!)
                                 &si, &pi);

        if (ok == 0) {
            Log::err("CreateProcessW failed from stage2, error: {}", GetLastError());
        } else {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    Log::init("updater");

    if (argc < 2) {
        Log::err("Arguments not enough, argc: {}/2", argc);
        return 1;
    }

    const std::wstring entry(argv[1]);

    if (entry == L"--stage1") {
        if (argc < 6) { // NOLINT(readability-magic-numbers)
            Log::err("Stage1: Arguments not enough, argc: {}/7", argc);
            return 1;
        }

        handleStage1(argv);
    } else if (entry == L"--stage2") {
        if (argc < 6) { // NOLINT(readability-magic-numbers)
            Log::err("Stage2: Arguments not enough, argc: {}/7", argc);
            return 1;
        }

        handleStage2(argv);
    } else {
        Log::err("Invalid entry");
        return 1;
    }

    return 0;
}
