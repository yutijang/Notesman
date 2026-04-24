#include "FileAssociation.hpp"

#ifdef Q_OS_WIN
#include "Logger.hpp"

#include <QCoreApplication>
#include <QString>
#include <filesystem>
#include <shlobj.h> // SHChangeNotify
#include <string>
#include <windows.h>

namespace {
    // Registry key layout (HKCU):
    //
    // Software\\Classes\\.rvpk
    //     (Default) = "Notesman.rvpk"
    //
    // Software\\Classes\\Notesman.rvpk
    //     (Default) = "Notesman Resource Pack"
    //
    // Software\\Classes\\Notesman.rvpk\\DefaultIcon
    //     (Default) = "C:\path\to\Notesman.exe,0"
    //
    // Software\\Classes\\Notesman.rvpk\\shell\\open\\command
    //     (Default) = "\"C:\\path\\to\\Notesman.exe\" --open-packer \"%1\""

    constexpr wchar_t const* K_EXT_KEY = L"Software\\Classes\\.rvpk";
    constexpr wchar_t const* K_PROGID_KEY = L"Software\\Classes\\Notesman.rvpk";
    constexpr wchar_t const* K_PROGID = L"Notesman.rvpk";
    constexpr wchar_t const* K_DESCRIPTION = L"Notesman Resource Pack";

    // Ghi một string value vào registry key (HKCU)
    // subKey: relative path từ HKCU
    // valueName: tên value (nullptr hoặc L"" = Default value)
    // data: string cần ghi
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    [[nodiscard]] bool writeRegString(wchar_t const* subKey, wchar_t const* valueName,
                                      std::wstring const& data) {
        HKEY hKey = nullptr;
        DWORD dispostion = 0;

        LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER, subKey, 0, nullptr, REG_OPTION_NON_VOLATILE,
                                  KEY_SET_VALUE, nullptr, &hKey, &dispostion);
        if (rc != ERROR_SUCCESS) {
            Log::err("RegCreateKeyEx failed for {}, error {}",
                     reinterpret_cast<char const*>(subKey), rc);
            return false;
        }

        // size tính bằng bytes, bao gồm null terminator
        DWORD const byteSize = static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t));

        rc = RegSetValueExW(hKey, valueName, 0, REG_SZ, reinterpret_cast<BYTE const*>(data.c_str()),
                            byteSize);
        RegCloseKey(hKey);

        if (rc != ERROR_SUCCESS) {
            Log::err("RegSetValueEx failed, error {}", rc);
            return false;
        }
        return true;
    }

    // Đọc Default value của một key
    // Trả về empty string nếu key không tồn tại hoặc lỗi
    [[nodiscard]] std::wstring readRegDefault(wchar_t const* subKey) {
        HKEY hKey = nullptr;
        LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_QUERY_VALUE, &hKey);
        if (rc != ERROR_SUCCESS) { return {}; }

        wchar_t buffer[1024] = {}; // NOLINT(readability-magic-numbers)
        DWORD bufSize = sizeof(buffer);
        DWORD type = 0;

        rc = RegQueryValueExW(hKey, nullptr, nullptr, &type, reinterpret_cast<BYTE*>(buffer),
                              &bufSize);

        RegCloseKey(hKey);

        if (rc != ERROR_SUCCESS || type != REG_SZ) { return {}; }
        return {buffer};
    }

    // Xoá registry key và toàn bộ subkey (recursive)
    void deleteRegKeyRecursive(wchar_t const* subKey) {
        // RegDeleteTreeW available từ Vista trở lên
        RegDeleteTreeW(HKEY_CURRENT_USER, subKey);
    }

    [[nodiscard]] std::wstring currentExePath() {
        std::filesystem::path const corePath =
            QCoreApplication::applicationFilePath().toStdWString();
        std::filesystem::path launcherPath = corePath.parent_path() / L"Notesman.exe";
        return launcherPath.make_preferred().wstring();
    }
} // namespace

namespace FileAssociation {
    bool isUpToDate() {
        // Kiểm tra .rvpk → ProgID
        std::wstring const progId = readRegDefault(K_EXT_KEY);
        if (progId != K_PROGID) { return false; }

        // Kiểm tra command path khớp với exe đang chạy
        std::wstring const commandKey = std::wstring(K_PROGID_KEY) + L"\\shell\\open\\command";
        std::wstring const registeredCmd = readRegDefault(commandKey.c_str());
        if (registeredCmd.empty()) { return false; }

        std::wstring const exePath = currentExePath();

        // registeredCmd có format: "\"<path>\" --open-packer \"%1\""
        // chỉ cần kiểm tra exePath có xuất hiện trong command không
        return registeredCmd.contains(exePath);
    }

    bool registerAssociation() {
        std::wstring const exePath = currentExePath();

        // .rvpk → ProgID
        if (!writeRegString(K_EXT_KEY, nullptr, K_PROGID)) { return false; }

        // ProgID description
        if (!writeRegString(K_PROGID_KEY, nullptr, K_DESCRIPTION)) { return false; }

        // DefaultIcon — dùng icon index 0 của exe
        std::wstring const iconValue = L"\"" + exePath + L"\",0";
        std::wstring const iconKey = std::wstring(K_PROGID_KEY) + L"\\DefaultIcon";
        if (!writeRegString(iconKey.c_str(), nullptr, iconValue)) { return false; }

        // shell\\open\\command
        std::wstring const command = L"\"" + exePath + L"\" --open-packer \"%1\"";
        std::wstring const commandKey = std::wstring(K_PROGID_KEY) + L"\\shell\\open\\command";
        if (!writeRegString(commandKey.c_str(), nullptr, command)) { return false; }

        // Notify shell để refresh icon cache ngay lập tức
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

        Log::info("registered for exe: {}", QString::fromStdWString(exePath).toStdString());
        return true;
    }

    void unregisterAssociation() {
        deleteRegKeyRecursive(K_PROGID_KEY);
        deleteRegKeyRecursive(K_EXT_KEY);
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
        Log::info("unregistered.");
    }
} // namespace FileAssociation
#endif
