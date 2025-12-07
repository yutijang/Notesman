#include <iostream>
#include <unordered_set>
#include <string>
#include <filesystem>
#include <system_error>
#include <windows.h>
#include <miniz.h>

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

    void clearFolder(const std::wstring &targetFolder) {
        const std::unordered_set<std::wstring> keepFiles{L"temp_update", L"data.db", L"config.ini"};

        fs::path dirPath(targetFolder);
        std::error_code ec;
        if (!fs::exists(dirPath, ec) || !fs::is_directory(dirPath, ec)) {
            std::wcerr << L"Folder invalid: " << targetFolder << L" - error: "
                       << std::wstring(ec.message().begin(), ec.message().end()) << L"\n";
            return;
        }

        for (const auto &entry : fs::directory_iterator(dirPath, ec)) {
            if (ec) {
                std::wcerr << L"Error: " << std::wstring(ec.message().begin(), ec.message().end())
                           << L"\n";
                ec.clear();
                continue;
            }

            const auto &path = entry.path();
            const auto name = path.filename().wstring();

            if (keepFiles.contains(name)) { continue; }

            if (entry.is_directory(ec)) {
                fs::remove_all(path, ec);
                if (ec) {
                    std::wcerr << L"Error remove all - "
                               << std::wstring(ec.message().begin(), ec.message().end()) << L"\n";
                }
            } else {
                fs::remove(path, ec);
                if (ec) {
                    std::wcerr << L"Error remove - "
                               << std::wstring(ec.message().begin(), ec.message().end()) << L"\n";
                }
            }
            ec.clear();
        }
    }

    bool unzipToFolder(const std::wstring &zipPath, const fs::path &destFolder, bool overwrite) {
        if (!fs::exists(zipPath)) { return false; }
        fs::create_directories(destFolder);

        std::string zipPathUtf8 = wstringToUtf8(zipPath);

        mz_zip_archive zip{};
        mz_zip_zero_struct(&zip);
        if (mz_zip_reader_init_file(&zip, zipPathUtf8.c_str(), 0) == 0) { return false; }

        const mz_uint num = mz_zip_reader_get_num_files(&zip);
        for (mz_uint i = 0; i < num; ++i) {
            mz_zip_archive_file_stat st;
            if (mz_zip_reader_file_stat(&zip, i, &st) == 0) {
                mz_zip_reader_end(&zip);
                return false;
            }
            fs::path outPath = destFolder / st.m_filename;
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
            std::cerr << "copyRecursive failed: source not found: " << from << "\n";
            return false;
        }

        std::error_code ec;

        fs::create_directories(to, ec);
        if (ec) {
            std::cerr << "copyRecursive failed: cannot create target directory: " << to << " : "
                      << ec.message() << "\n";
            return false;
        }

        fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);

        if (ec) {
            std::cerr << "copyRecursive failed: " << ec.message() << " (code " << ec.value() << ")"
                      << " while copying from " << from << " to " << to << "\n";
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

} // namespace

int wmain([[maybe_unused]] int argc, [[maybe_unused]] wchar_t* argv[]) {
    if (argc < 2) {
        std::wcerr << "Arguments not enough\n";
        return 1;
    }

    const std::wstring argvWs(argv[1]);
    const std::string entry(argvWs.begin(), argvWs.end());

    if (entry == "--stage1") {
        if (argc < 6) {
            std::cerr << "Arguments not enough\n";
            return 1;
        }

        // arguments received from main app
        // argv[1] = --stage1
        // argv[2] = app PID
        // argv[3] = app dir
        // argv[4] = zip path
        // argv[5] = app name

        const auto appPID = std::stoul(argv[2]);
        waitForProcessExit(appPID);

        const std::wstring appDir(argv[3]);
        const std::wstring tempDir = appDir + L"\\temp_update";
        const std::wstring zipPath(argv[4]);
        unzipToFolder(zipPath, tempDir, false);

        // prepare arguments send to stage 2
        // argv[1] = --stage2
        // argv[2] = PID stage1 (current process: updater.exxe)
        // argv[3] = app dir
        // argv[4] = app name
        // argv[5] = zip path

        const std::wstring exePath = tempDir + L"\\updater.exe";
        const std::wstring entryForStage2{L"--stage2"};
        const std::wstring currentPID = std::to_wstring(getCurrentProcessId());
        const std::wstring appName(argv[5]);

        const std::wstring cmdLine = L"\"" + exePath + L"\" " // "C:\...\temp_update\updater.exe"
                                   + entryForStage2 + L" "    // --stage2
                                   + currentPID + L" "        // 1234
                                   + L"\"" + appDir + L"\" "  // "C:\Apps\Notesman"
                                   + L"\"" + appName + L"\" " // "Notesman.exe"
                                   + L"\"" + zipPath + L"\""; // "C:\Apps\update.zip"
                                                              //
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
            std::cerr << "CreateProcessW failed: " << GetLastError() << "\n";
        } else {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    } else if (entry == "--stage2") {
        if (argc < 6) {
            std::cerr << "Arguments not enough\n";
            return 1;
        }

        // arguments received from stage 1
        // argv[1] = --stage2
        // argv[2] = PID stage1 (current process: updater.exxe)
        // argv[3] = app dir
        // argv[4] = app name
        // argv[5] = zip path

        const auto stage1PID = std::stoul(argv[2]);
        waitForProcessExit(stage1PID);

        const std::wstring appDir(argv[3]);
        // delete all file/folder in app dir except temp_update, data.db, config.ini
        clearFolder(appDir);

        const std::wstring tempDir = appDir + L"\\temp_update";
        copyRecursive(tempDir, appDir);

        // prepare arguments send to main app
        // argv[1] = --update-done
        // argv[2] = PID stage2 (current process: updater.exxe)
        // argv[3] = temp_update dir path for delete
        // argv[4] = zip path

        const std::wstring entryForUpdateDone{L"--update-done"};
        const std::wstring currentPID = std::to_wstring(getCurrentProcessId());
        const std::wstring appName(argv[4]);
        const std::wstring zipPath(argv[5]);

        const std::wstring exePath = appDir + L"\\" + appName;

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
            std::cerr << "CreateProcessW failed: " << GetLastError() << "\n";
        } else {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    } else {
        std::cerr << "Invalid entry\n";
        return 1;
    }

    return 0;
}
