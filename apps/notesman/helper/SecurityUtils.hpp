#pragma once

#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shell32.lib")

namespace security_utils {
    // Key và Secret dùng chung
    static constexpr uint8_t OBFUSCATION_KEY = 0xa4U;
    static constexpr unsigned char OBFUSCATED_SECRET[] = {0xef, 0xc9, 0xec, 0x87, 0xc2, 0xc1,
                                                          0xf2, 0xdd, 0x99, 0xfd, 0xd4, 0xf9,
                                                          0xe8, 0xd4, 0xdc, 0x8d, 0x00};

    // Giải mã secret tại chỗ (In-place deobfuscation)
    inline void getSecret(char* outStr) {
        for (std::size_t i = 0; i < (sizeof(OBFUSCATED_SECRET) - 1); ++i) {
            outStr[i] = static_cast<char>(OBFUSCATED_SECRET[i] ^ OBFUSCATION_KEY);
        }
        outStr[sizeof(OBFUSCATED_SECRET) - 1] = '\0';
    }

    // Lấy Epoch Minutes (Timestamp theo phút)
    inline long long getCurrentEpochMinutes() {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        unsigned __int64 val = ((unsigned __int64) ft.dwHighDateTime << 32) | ft.dwLowDateTime;
        // Chuyển từ 100-nanosecond intervals sang phút: 10 * 1000 * 1000 * 60
        return static_cast<long long>(val / 600000000ULL);
    }

    // Tính HMAC-SHA256 sử dụng BCrypt (Win32 API chuẩn)
    inline BOOL computeHMAC(const char* key, const char* message, BYTE* outHash) {
        BCRYPT_ALG_HANDLE hAlg{};
        BCRYPT_HASH_HANDLE hHash{};
        DWORD cbHash{};
        DWORD cbData{};

        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr,
                                        BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) {
            return FALSE;
        }

        BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE) &cbHash, sizeof(DWORD), &cbData, 0);

        if (BCryptCreateHash(hAlg, &hHash, nullptr, 0,
                             reinterpret_cast<PUCHAR>(const_cast<char*>(key)),
                             static_cast<ULONG>(lstrlenA(key)), 0) == 0) {
            BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<char*>(message)),
                           static_cast<ULONG>(lstrlenA(message)), 0);
            BCryptFinishHash(hHash, outHash, cbHash, 0);
            BCryptDestroyHash(hHash);
        }

        BCryptCloseAlgorithmProvider(hAlg, 0);
        return TRUE;
    }

    inline bool verifyLauncherToken() {
        int argcW{};
        wchar_t** argvW = CommandLineToArgvW(GetCommandLineW(), &argcW);
        if (argcW < 2) { return false; }

        wchar_t* token = argvW[1];
        wchar_t* sep = wcschr(token, L':');
        if (sep == nullptr) { return false; }

        wchar_t tsW[32] = {0};
        lstrcpynW(tsW, token, (int) (sep - token + 1));
        long long receivedTs = _wtoi64(tsW);
        wchar_t* receivedHex = sep + 1;

        char secret[32];
        security_utils::getSecret(secret);

        bool valid = false;
        // Kiểm tra cửa sổ thời gian +/- 1 phút để tránh lệch clock
        for (long long t = receivedTs - 1; t <= receivedTs + 1; ++t) {
            char testTsA[32];
            wsprintfA(testTsA, "%lld", t);

            BYTE hash[32];
            security_utils::computeHMAC(secret, testTsA, hash);

            wchar_t expectedHex[65] = {0};
            for (int i = 0; i < 32; ++i) { wsprintfW(&expectedHex[i * 2], L"%02x", hash[i]); }

            if (lstrcmpiW(receivedHex, expectedHex) == 0) {
                valid = true;
                break;
            }
        }

        LocalFree(argvW);
        return valid;
    }
} // namespace security_utils
