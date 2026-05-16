#include "application/FileAssociation.hpp"

#include "common/logger/Logger.hpp"

#include <QCoreApplication>
#include <QString>
#include <filesystem>
#include <string>

#ifdef Q_OS_WIN

#include <ShlObj.h> // SHChangeNotify
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
[[nodiscard]] bool
    writeRegString(wchar_t const* subKey, wchar_t const* valueName, std::wstring const& data) {
    HKEY hKey = nullptr;
    DWORD dispostion = 0;

    LONG rc = RegCreateKeyExW(HKEY_CURRENT_USER,
                              subKey,
                              0,
                              nullptr,
                              REG_OPTION_NON_VOLATILE,
                              KEY_SET_VALUE,
                              nullptr,
                              &hKey,
                              &dispostion);
    if (rc != ERROR_SUCCESS) {
        Log::err(
            "RegCreateKeyEx failed for {}, error {}", reinterpret_cast<char const*>(subKey), rc);
        return false;
    }

    // size tính bằng bytes, bao gồm null terminator
    DWORD const byteSize = static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t));

    rc = RegSetValueExW(
        hKey, valueName, 0, REG_SZ, reinterpret_cast<BYTE const*>(data.c_str()), byteSize);
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
    if (rc != ERROR_SUCCESS) {
        return {};
    }

    wchar_t buffer[1024] = {}; // NOLINT(readability-magic-numbers)
    DWORD bufSize = sizeof(buffer);
    DWORD type = 0;

    rc = RegQueryValueExW(hKey, nullptr, nullptr, &type, reinterpret_cast<BYTE*>(buffer), &bufSize);

    RegCloseKey(hKey);

    if (rc != ERROR_SUCCESS || type != REG_SZ) {
        return {};
    }
    return {buffer};
}

// Xoá registry key và toàn bộ subkey (recursive)
void deleteRegKeyRecursive(wchar_t const* subKey) {
    // RegDeleteTreeW available từ Vista trở lên
    RegDeleteTreeW(HKEY_CURRENT_USER, subKey);
}

[[nodiscard]] std::wstring currentExePath() {
    std::filesystem::path const corePath = QCoreApplication::applicationFilePath().toStdWString();
    std::filesystem::path launcherPath = corePath.parent_path() / L"Notesman.exe";
    return launcherPath.make_preferred().wstring();
}

} // namespace

namespace FileAssociation {

bool isUpToDate() {
    // Kiểm tra .rvpk → ProgID
    std::wstring const progId = readRegDefault(K_EXT_KEY);
    if (progId != K_PROGID) {
        return false;
    }

    // Kiểm tra command path khớp với exe đang chạy
    std::wstring const commandKey = std::wstring(K_PROGID_KEY) + L"\\shell\\open\\command";
    std::wstring const registeredCmd = readRegDefault(commandKey.c_str());
    if (registeredCmd.empty()) {
        return false;
    }

    std::wstring const exePath = currentExePath();

    // registeredCmd có format: "\"<path>\" --open-packer \"%1\""
    // chỉ cần kiểm tra exePath có xuất hiện trong command không
    return registeredCmd.contains(exePath);
}

bool registerAssociation() {
    std::wstring const exePath = currentExePath();

    // .rvpk → ProgID
    if (!writeRegString(K_EXT_KEY, nullptr, K_PROGID)) {
        return false;
    }

    // ProgID description
    if (!writeRegString(K_PROGID_KEY, nullptr, K_DESCRIPTION)) {
        return false;
    }

    // DefaultIcon — dùng icon index 0 của exe
    std::wstring const iconValue = L"\"" + exePath + L"\",0";
    std::wstring const iconKey = std::wstring(K_PROGID_KEY) + L"\\DefaultIcon";
    if (!writeRegString(iconKey.c_str(), nullptr, iconValue)) {
        return false;
    }

    // shell\\open\\command
    std::wstring const command = L"\"" + exePath + L"\" --open-packer \"%1\"";
    std::wstring const commandKey = std::wstring(K_PROGID_KEY) + L"\\shell\\open\\command";
    if (!writeRegString(commandKey.c_str(), nullptr, command)) {
        return false;
    }

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

#ifdef Q_OS_LINUX
#include <QProcess>
#include <QStringList>
#include <cstdlib>
#include <fstream>
#include <system_error>

namespace {

// XDG paths (user-local, không cần root)
// ~/.local/share/mime/packages/notesman-rvpk.xml
// ~/.local/share/applications/notesman-rvpk.desktop

constexpr char const* K_MIME_TYPE = "application/x-notesman-rvpk";
constexpr char const* K_DESKTOP_ID = "notesman-rvpk.desktop";
constexpr char const* K_MIME_PKG_NAME = "notesman-rvpk.xml";

[[nodiscard]] std::filesystem::path xdgDataHome() {
    char const* xdgEnv = std::getenv("XDG_DATA_HOME");
    if ((xdgEnv != nullptr) && xdgEnv[0] != '\0') {
        return xdgEnv;
    }

    char const* home = std::getenv("HOME");

    return (home != nullptr) ? std::filesystem::path(home) / ".local/share"
                             : std::filesystem::path("/tmp");
}

[[nodiscard]] std::filesystem::path mimePackagePath() {
    return xdgDataHome() / "mime/packages" / K_MIME_PKG_NAME;
}

[[nodiscard]] std::filesystem::path desktopFilePath() {
    return xdgDataHome() / "applications" / K_DESKTOP_ID;
}

[[nodiscard]] std::string currentExePath() {
    std::filesystem::path const corePath = QCoreApplication::applicationFilePath().toStdString();
    std::filesystem::path const launcherPath = corePath.parent_path() / "Notesman";

    return launcherPath.lexically_normal().string();
}

// Chạy command, trả về true nếu exit code = 0
[[nodiscard]] bool runCommand(QString const& program, QStringList const& args) {
    QProcess proc;

    proc.start(program, args);
    proc.waitForFinished(5000); // timeout 5 giây // NOLINT(readability-magic-numbers)
    bool const ok = (proc.exitCode() == 0);
    if (!ok) {
        Log::err("Command failed: {} {}, exit={}",
                 program.toStdString(),
                 args.join(' ').toStdString(),
                 proc.exitCode());
    }

    return ok;
}

[[nodiscard]] std::filesystem::path localIconPath() {
    // ~/.local/share/icons/hicolor/256x256/apps/notesman.png
    return xdgDataHome() / "icons/hicolor/256x256/apps/notesman.png";
}

// Copy icon từ AppImage mount point ra ~/.local/share/icons/
// QCoreApplication::applicationDirPath() trỏ vào mount point của AppImage
[[nodiscard]] bool installIcon() {
    std::filesystem::path const srcIcon =
        std::filesystem::path(QCoreApplication::applicationDirPath().toStdString()) /
        "usr/share/icons/hicolor/256x256/apps/notesman.png";

    std::filesystem::path const dstIcon = localIconPath();

    if (!std::filesystem::exists(srcIcon)) {
        Log::err("Icon not found in AppImage at: {}", srcIcon.string());
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(dstIcon.parent_path(), ec);
    std::filesystem::copy_file(
        srcIcon, dstIcon, std::filesystem::copy_options::overwrite_existing, ec);

    if (ec) {
        Log::err("Failed to copy icon: {}", ec.message());
        return false;
    }

    return true;
}

[[nodiscard]] bool writeMimePackage(std::filesystem::path const& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream f(path);

    if (!f) {
        return false;
    }

    f << R"(<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
  <mime-type type=")"
      << K_MIME_TYPE << R"(">
    <comment>Notesman Resource Pack</comment>
    <glob pattern="*.rvpk"/>
    <icon name="notesman"/>
  </mime-type>
</mime-info>
)";
    return f.good();
}

[[nodiscard]] bool writeDesktopFile(std::filesystem::path const& path, std::string const& exePath) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream f(path);
    if (!f) {
        return false;
    }

    // Icon dùng tên "notesman" — XDG tự tìm trong hicolor theme
    f << "[Desktop Entry]\n"
      << "Version=1.1\n"
      << "Type=Application\n"
      << "Name=Notesman\n"
      << "Exec=" << exePath << " --open-packer %f\n"
      << "Icon=notesman\n"
      << "MimeType=" << K_MIME_TYPE << ";\n"
      << "NoDisplay=true\n"
      << "StartupNotify=false\n";

    return f.good();
}

// Đọc Exec= line từ .desktop file để so sánh path
[[nodiscard]] std::string readDesktopExecPath(std::filesystem::path const& path) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.starts_with("Exec=")) {
            continue;
        }

        // "Exec=/path/to/Notesman --open-packer %f"
        std::string const execLine = line.substr(5); // bỏ "Exec="
        std::string const space = " ";
        auto const spacePos = execLine.find(space);

        return (spacePos != std::string::npos) ? execLine.substr(0, spacePos) : execLine;
    }

    return {};
}

} // namespace

namespace FileAssociation {

bool isUpToDate() {
    std::filesystem::path const desktop = desktopFilePath();
    if (!std::filesystem::exists(desktop)) {
        return false;
    }
    if (!std::filesystem::exists(mimePackagePath())) {
        return false;
    }

    std::string const registeredExe = readDesktopExecPath(desktop);

    return registeredExe == currentExePath();
}

bool registerAssociation() {
    std::string const exePath = currentExePath();

    if (!installIcon()) {
        return false;
    }

    if (!writeMimePackage(mimePackagePath())) {
        Log::err("Failed to write MIME package");
        return false;
    }

    if (!writeDesktopFile(desktopFilePath(), exePath)) {
        Log::err("Failed to write .desktop file");
        return false;
    }

    // refresh icon cache
    std::string const iconDir = (xdgDataHome() / "icons").string();
    bool runOk = runCommand("gtk-update-icon-cache", {"-f", "-t", QString::fromStdString(iconDir)});
    if (!runOk) {
        Log::err("gtk-update-icon-cache failed");
    }

    std::string const mimeDir = (xdgDataHome() / "mime").string();
    if (!runCommand("update-mime-database", {QString::fromStdString(mimeDir)})) {
        Log::err("update-mime-database failed");
        return false;
    }

    std::string const appsDir = (xdgDataHome() / "applications").string();
    runOk = runCommand("update-desktop-database", {QString::fromStdString(appsDir)});
    if (!runOk) {
        Log::err("update-desktop-database failed");
    }

    Log::info("registered for exe: {}", exePath);
    return true;
}

void unregisterAssociation() {
    std::error_code ec;
    std::filesystem::remove(mimePackagePath(), ec);
    std::filesystem::remove(desktopFilePath(), ec);
    std::filesystem::remove(localIconPath(), ec);

    std::string const mimeDir = (xdgDataHome() / "mime").string();
    bool runOk = runCommand("update-mime-database", {QString::fromStdString(mimeDir)});
    if (!runOk) {
        Log::err("update-mime-database failed");
    }

    std::string const appsDir = (xdgDataHome() / "applications").string();
    runOk = runCommand("update-desktop-database", {QString::fromStdString(appsDir)});
    if (!runOk) {
        Log::err("update-desktop-database failed");
    }

    Log::info("unregistered.");
}

} // namespace FileAssociation
#endif
