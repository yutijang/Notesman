// #include <windows.h>
#include <string>

#include "simple_log.hpp"

namespace simple_log {
    void write(const std::wstring &msg) {
        const wchar_t* path = L"logs\\launcher.log";

        // 1. Lấy thời gian hệ thống
        SYSTEMTIME st;
        GetLocalTime(&st);

        // 2. Định dạng chuỗi thời gian: [YYYY-MM-DD HH:MM:SS]
        wchar_t timestamp[32];
        swprintf(timestamp, 32, L"[%04d-%02d-%02d %02d:%02d:%02d] ", st.wYear, st.wMonth, st.wDay,
                 st.wHour, st.wMinute, st.wSecond);

        // 3. Mở file log
        HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);

        if (h == INVALID_HANDLE_VALUE) { return; }

        DWORD written{};

        // Ghi Timestamp
        auto bytesToWrite = static_cast<DWORD>(
            static_cast<unsigned long long>(lstrlenW(timestamp)) * sizeof(wchar_t));
        WriteFile(h, timestamp, bytesToWrite, &written, nullptr);

        // Ghi nội dung log chính
        WriteFile(h, msg.c_str(), static_cast<DWORD>(msg.size() * sizeof(wchar_t)), &written,
                  nullptr);

        // Ghi xuống dòng
        WriteFile(h, L"\r\n", 4, &written, nullptr);

        CloseHandle(h);
    }
} // namespace simple_log
