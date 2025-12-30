#ifdef _DEBUG
constexpr wchar_t QT_PLATFORM_DLL[] = L"platforms\\qwindowsd.dll";
#else
constexpr wchar_t QT_PLATFORM_DLL[] = L"platforms\\qwindows.dll";
#endif

#include <cstddef>
#include <string>
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>

#include "simple_log.hpp"
#include "SecurityUtils.hpp"

static BOOL fileExists(const wchar_t* name) {
    DWORD attr = GetFileAttributesW(name);

    return static_cast<BOOL>(attr != INVALID_FILE_ATTRIBUTES);
}

static void showMessage(LPCWSTR text, LPCWSTR title) {
    MessageBoxW(nullptr, text, title, MB_OK | MB_ICONERROR);
}

static void zeroMemoryW(void* ptr, SIZE_T size) noexcept {
    auto* p = static_cast<BYTE*>(ptr);
    while ((size--) != 0U) { *p++ = 0; } // NOLINT
}

// Hàm bổ trợ để xóa bit đánh dấu của HMODULE
inline std::size_t getBaseAddr(HMODULE hMod) noexcept {
    return (std::size_t) hMod & ~0x3; // NOLINT
}

static BOOL isAlreadyInList(const wchar_t* list, const wchar_t* name) {
    if (list == nullptr || list[0] == L'\0') { return FALSE; }

    // Nếu tìm thấy chuỗi 'name' nằm trong 'list'
    if (StrStrW(list, name) != nullptr) { return TRUE; }
    return FALSE;
}

static BOOL isLibraryAvailable(const wchar_t* dllName, int depth, wchar_t* outMissingName) {
    if ((dllName == nullptr) || dllName[0] == L'\0' || depth < 0) { return FALSE; }

    // Thêm LOAD_WITH_ALTERED_SEARCH_PATH để Windows tìm DLL con cùng thư mục với DLL cha
    HMODULE hMod = LoadLibraryExW(dllName, nullptr,
                                  DONT_RESOLVE_DLL_REFERENCES | LOAD_WITH_ALTERED_SEARCH_PATH);

    if (hMod == nullptr) {
        // Nếu thất bại ở đây -> không tìm thấy chính DLL này
        if (outMissingName != nullptr && outMissingName[0] == L'\0') {
            lstrcpyW(outMissingName, dllName);
        }
        return FALSE;
    }

    std::size_t baseAddr = getBaseAddr(hMod);
    if (depth > 0) {
        PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER) baseAddr;
        PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS) (baseAddr + dos->e_lfanew);

        // Kiểm tra an toàn trước khi truy cập DataDirectory
        if (nt->OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_IMPORT) {
            DWORD rva =
                nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
            if (rva != 0) {
                PIMAGE_IMPORT_DESCRIPTOR imp = (PIMAGE_IMPORT_DESCRIPTOR) (baseAddr + rva);
                while (imp->Name != 0U) {
                    LPCSTR subDllA = (LPCSTR) (baseAddr + imp->Name);
                    wchar_t wSubName[MAX_PATH];

                    if (MultiByteToWideChar(CP_ACP, 0, subDllA, -1, wSubName, MAX_PATH) > 0) {
                        if (wSubName[0] != L'\0' && lstrcmpiW(wSubName, L"kernel32.dll") != 0 &&
                            lstrcmpiW(wSubName, L"user32.dll") != 0) {
                            // Nếu tầng sâu báo lỗi
                            if (isLibraryAvailable(wSubName, depth - 1, outMissingName) == 0) {
                                // Nếu tầng dưới đã gán tên vào outMissingName rồi,
                                // tầng này không gán đè lên nữa
                                if (outMissingName != nullptr && outMissingName[0] == L'\0') {
                                    lstrcpyW(outMissingName, wSubName);
                                }
                                FreeLibrary(hMod);
                                return FALSE;
                            }
                        }
                    }
                    imp++;
                }
            }
        }
    }

    FreeLibrary(hMod);
    return TRUE;
}

static void appendAnotherMissing(wchar_t* missing, int maxLen, const wchar_t* name,
                                 const wchar_t* note = nullptr) {
    if (isAlreadyInList(missing, name) != 0) { return; }

    if (lstrlenW(missing) > 0 && lstrlenW(missing) < maxLen - 2) { lstrcatW(missing, L"\n"); }

    if (note != nullptr) {
        if (lstrlenW(missing) + lstrlenW(name) + lstrlenW(note) + 2 < maxLen) {
            lstrcatW(missing, L"  • ");
            lstrcatW(missing, name);
            lstrcatW(missing, note);
        }
    } else {
        if (lstrlenW(missing) + lstrlenW(name) < maxLen) {
            lstrcatW(missing, L"  • ");
            lstrcatW(missing, name);
        }
    }
}

/**
 * Duyệt danh sách Import của một file EXE/DLL và kiểm tra tính khả dụng.
 * @return: Số lượng thư viện bị thiếu.
 */
static int checkDependencies(LPCWSTR exePath, wchar_t* missing, int maxLen) {
    // Dùng DONT_RESOLVE_DLL_REFERENCES để đảm bảo Header được ánh xạ đúng
    HMODULE hMod = LoadLibraryExW(exePath, nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (hMod == nullptr) { return -1; }

    // Ép kiểu chuẩn xác: Một số flag của LoadLibrary có thể set bit thấp của handle
    // cần xóa bit đó để lấy địa chỉ bộ nhớ thực tế
    std::size_t baseAddr = (std::size_t) hMod & ~0x3; // NOLINT

    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER) baseAddr;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        FreeLibrary(hMod);
        return -1;
    }

    PIMAGE_NT_HEADERS nt = (PIMAGE_NT_HEADERS) (baseAddr + dos->e_lfanew);

    // Kiểm tra tính hợp lệ của NT Header
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        FreeLibrary(hMod);
        return -1;
    }

    DWORD rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    if (rva == 0U) {
        FreeLibrary(hMod);
        return 0;
    }

    PIMAGE_IMPORT_DESCRIPTOR imp = (PIMAGE_IMPORT_DESCRIPTOR) (baseAddr + rva);
    int count{};

    while (imp->Name != 0U) {
        LPCSTR dllNameA = (LPCSTR) (baseAddr + imp->Name);

        // 1. Kiểm tra an toàn địa chỉ ANSI
        if (IsBadStringPtrA(dllNameA, MAX_PATH) != 0) {
            imp++;
            continue;
        }

        wchar_t wName[MAX_PATH];
        zeroMemoryW(wName, sizeof(wName));

        // 2. Chuyển đổi và kiểm tra kết quả trả về của API
        int convertedChars = MultiByteToWideChar(CP_ACP, 0, dllNameA, -1, wName, MAX_PATH);

        if (convertedChars > 1) { // > 1 vì tính cả ký tự null kết thúc
            wchar_t actualMissing[MAX_PATH];
            zeroMemoryW(actualMissing, sizeof(actualMissing));

            if (isLibraryAvailable(wName, 1, actualMissing) == 0) {
                const wchar_t* finalName = (actualMissing[0] != L'\0') ? actualMissing : wName;

                int nameLen = lstrlenW(finalName);
                if (nameLen > 0 && finalName[0] >= 32) {
                    // Chỉ thêm nếu chưa có trong danh sách (ngăn trùng lặp tên thư viện)
                    if (isAlreadyInList(missing, finalName) == 0) {
                        if (lstrlenW(missing) > 0 && lstrlenW(missing) < maxLen - 2) {
                            lstrcatW(missing, L"\n");
                        }

                        if (lstrlenW(missing) + nameLen < maxLen) {
                            lstrcatW(missing, L"  • ");
                            lstrcatW(missing, finalName);
                            count++;
                        }
                    }
                }
            }
        }
        imp++;
    }

    if (fileExists(QT_PLATFORM_DLL) == 0) {
        appendAnotherMissing(missing, maxLen, QT_PLATFORM_DLL, L" (Missing Qt Platform)");
        count++;
    }

    if (fileExists(L"libssl-3-x64.dll") == 0) {
        appendAnotherMissing(missing, maxLen, L"libssl-3-x64.dll");
        count++;
    }

    FreeLibrary(hMod);
    return count;
}

static void callMainCore(const wchar_t* filenameCore) {
    SetFileAttributesW(filenameCore, FILE_ATTRIBUTE_HIDDEN);

    char secret[32];
    security_utils::getSecret(secret);

    char tsStr[32];
    wsprintfA(tsStr, "%lld", security_utils::getCurrentEpochMinutes());

    BYTE hash[32];
    security_utils::computeHMAC(secret, tsStr, hash);

    wchar_t hexPart[65] = {0};
    for (int i = 0; i < 32; ++i) {
        wsprintfW(&hexPart[static_cast<ptrdiff_t>(i * 2)], L"%02x", hash[i]);
    }

    wchar_t tokenW[128];
    wsprintfW(tokenW, L"%S:%s", tsStr, hexPart); // %S (viết hoa) để format char* sang wchar_t*

    // handle arguments
    int argcL{};
    wchar_t** argvL = CommandLineToArgvW(GetCommandLineW(), &argcL);
    wchar_t extraArgs[2048] = L"";

    // Bắt đầu từ 1 để bỏ qua chính cái tên "Launcher.exe"
    for (int i = 1; i < argcL; ++i) {
        lstrcatW(extraArgs, L" ");
        lstrcatW(extraArgs, L"\""); // Bọc nháy kép để tránh lỗi đường dẫn có khoảng trắng
        lstrcatW(extraArgs, argvL[i]);
        lstrcatW(extraArgs, L"\"");
    }
    LocalFree(argvL);

    wchar_t cmdLine[4096];
    wsprintfW(cmdLine, L"\"%s\" \"%s\"%s", filenameCore, tokenW, extraArgs);

    STARTUPINFOW si;
    zeroMemoryW(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;

    PROCESS_INFORMATION pi;
    zeroMemoryW(&pi, sizeof(pi));

    DWORD flags = CREATE_NO_WINDOW | DETACHED_PROCESS;

    if (CreateProcessW(nullptr, cmdLine, nullptr, nullptr, FALSE, flags, nullptr, nullptr, &si,
                       &pi) == 0) {
        simple_log::write(L"Không thể khởi chạy " + std::wstring(filenameCore));
        showMessage((std::wstring(L"Không thể khởi chạy ") + filenameCore).c_str(), L"Lỗi");

        return;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

int WINAPI wWinMain(HINSTANCE /*unused*/, HINSTANCE /*unused*/, PWSTR /*unused*/, int /*unused*/) {
    // Lấy đường dẫn tuyệt đối của Launcher
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    // Lấy đường dẫn thư mục (Directory)
    std::wstring appDir = exePath;
    size_t lastSlash = appDir.find_last_of(L"\\/");
    if (lastSlash != std::string::npos) { appDir = appDir.substr(0, lastSlash); }

    // Thiết lập CWD
    SetCurrentDirectoryW(appDir.c_str());

    constexpr int bufferSize{2048}; // Tăng kích thước buffer vì kiểm tra nhiều file
    wchar_t missing[bufferSize];
    zeroMemoryW(missing, sizeof(missing));
    missing[0] = L'\0';

    // Danh sách các file cần kiểm tra tính toàn vẹn
    std::wstring fullPathCore = appDir + L"\\NotesmanCore.dll";
    std::wstring fullPathUpdater = appDir + L"\\updater.exe";

    const wchar_t* filenameCore = fullPathCore.c_str();
    const wchar_t* targets[] = {filenameCore, fullPathUpdater.c_str()};

    int totalMissing{};
    BOOL isCrtMissing = FALSE; // Cờ đánh dấu thiếu CRT

    for (const wchar_t* exeName : targets) {
        // Kiểm tra xem file EXE có tồn tại hay không trước khi kiểm tra DLL
        if (fileExists(exeName) == 0) {
            if (totalMissing > 0) { lstrcatW(missing, L"\n"); }
            lstrcatW(missing, L"Không tìm thấy file: ");
            lstrcatW(missing, wcsrchr(exeName, L'\\') + 1); // Lấy tên file từ path
            totalMissing++;
            continue;
        }

        // Kiểm tra DLL cho từng file
        int missCount = checkDependencies(exeName, missing, bufferSize);

        if (missCount < 0) {
            simple_log::write(L"Lỗi hệ thống khi phân tích PE: " + std::wstring(exePath));
            showMessage(L"Không thể khởi chạy do lỗi truy cập tệp tin hệ thống.",
                        L"Lỗi nghiêm trọng");
            // mở log khi gặp lỗi hệ thống không xác định
            ShellExecuteW(nullptr, L"open", L"logs\\launcher.log", nullptr, nullptr, SW_SHOWNORMAL);
            return 1;
        }
        totalMissing += missCount;
    }

    // Nếu bất kỳ file nào thiếu phụ thuộc, thông báo và dừng lại
    if (totalMissing > 0) {
        simple_log::write(L"--- PHÁT HIỆN THIẾU PHỤ THUỘC (DEPENDENCY ERROR) ---");
        simple_log::write(missing);

        // Kiểm tra xem trong danh sách thiếu có các file của CRT không
        if ((StrStrW(missing, L"VCRUNTIME") != nullptr) ||
            (StrStrW(missing, L"api-ms-win-crt") != nullptr)) {
            isCrtMissing = TRUE;
        }

        if (isCrtMissing != 0) {
            int msgBox = MessageBoxW(
                nullptr,
                L"Ứng dụng thiếu thư viện C++ Runtime cần thiết.\n\n"
                L"Bạn có muốn cài đặt Microsoft Visual C++ Redistributable ngay bây giờ không?",
                L"Yêu cầu cài đặt thành phần hệ thống", MB_YESNO | MB_ICONQUESTION);

            if (msgBox == IDYES) {
                if (fileExists(L"VCRedist\\vc_redist.x64.exe") != 0) {
                    simple_log::write(L"Khởi chạy trình cài đặt vc_redist.x64.exe...");
                    ShellExecuteW(nullptr, L"open", L"VCRedist\\vc_redist.x64.exe",
                                  L"/passive /norestart", nullptr, SW_SHOWNORMAL);
                } else {
                    simple_log::write(L"Lỗi: Không tìm thấy file vc_redist.x64.exe để cài đặt.");
                    // Nếu không có file kèm theo, mở link tải chính thức
                    ShellExecuteW(nullptr, L"open",
                                  L"https://aka.ms/vs/17/release/vc_redist.x64.exe", nullptr,
                                  nullptr, SW_SHOWNORMAL);
                }
            }
        } else {
            constexpr wchar_t supportEmail[] = L"yutijang@gmail.com";

            std::wstring msg = L"Ứng dụng không thể khởi động vì thiếu các thành phần sau:\n\n";
            msg += missing;
            msg += L"\n\nHãy tải lại ứng dụng hoặc liên hệ hỗ trợ: ";
            msg += supportEmail;

            showMessage(msg.c_str(), L"Thiếu thư viện");
        }

        return 1;
    }

    // --- Nếu mọi thứ OK, tiến hành chạy ứng dụng chính ---
    callMainCore(filenameCore);

    return 0;
}
