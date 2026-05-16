#pragma once

// clang-format off
#include <windows.h>
#include <bcrypt.h>
// clang-format on

#include <array>
#include <cstddef>
#include <cstdint>
#include <shellapi.h>
#include <span>
#include <stdexcept>

namespace security_utils {

// Key và Secret dùng chung
static constexpr std::uint8_t OBFUSCATION_KEY = 0xa4U;
static constexpr unsigned char OBFUSCATED_SECRET[] = {0xef,
                                                      0xc9,
                                                      0xec,
                                                      0x87,
                                                      0xc2,
                                                      0xc1,
                                                      0xf2,
                                                      0xdd,
                                                      0x99,
                                                      0xfd,
                                                      0xd4,
                                                      0xf9,
                                                      0xe8,
                                                      0xd4,
                                                      0xdc,
                                                      0x8d,
                                                      0x00};

// Giải mã secret tại chỗ (In-place deobfuscation)
inline void getSecret(char* outStr) {
    for (std::size_t i = 0; i < (sizeof(OBFUSCATED_SECRET) - 1); ++i) {
        outStr[i] = static_cast<char>(OBFUSCATED_SECRET[i] ^ OBFUSCATION_KEY);
    }
    outStr[sizeof(OBFUSCATED_SECRET) - 1] = '\0';
}

// Lấy Epoch Minutes (Timestamp theo phút)
// NOLINTBEGIN (readability-magic-numbers)
inline long long getCurrentEpochMinutes() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    std::uint64_t val = (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) |
                        static_cast<std::uint64_t>(ft.dwLowDateTime);
    // Chuyển từ 100-nanosecond intervals sang phút: 10 * 1000 * 1000 * 60
    return static_cast<long long>(val / 600000000ULL);
}

// NOLINTEND

// Tính HMAC-SHA256 sử dụng BCrypt (Win32 API chuẩn)
inline BOOL computeHMAC(char const* key, char const* message, BYTE* outHash) {
    BCRYPT_ALG_HANDLE hAlg{};
    BCRYPT_HASH_HANDLE hHash{};
    ULONG cbHash{};
    ULONG cbData{};

    if (BCryptOpenAlgorithmProvider(
            &hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) {
        return FALSE;
    }

    if (BCryptGetProperty(hAlg,
                          BCRYPT_HASH_LENGTH,
                          reinterpret_cast<PUCHAR>(&cbHash),
                          sizeof(cbHash),
                          &cbData,
                          0) != 0) {
        return FALSE;
    }

    if (BCryptCreateHash(hAlg,
                         &hHash,
                         nullptr,
                         0,
                         reinterpret_cast<PUCHAR>(const_cast<char*>(key)),
                         static_cast<ULONG>(lstrlenA(key)),
                         0) == 0) {
        BCryptHashData(hHash,
                       reinterpret_cast<PUCHAR>(const_cast<char*>(message)),
                       static_cast<ULONG>(lstrlenA(message)),
                       0);
        BCryptFinishHash(hHash, outHash, cbHash, 0);
        BCryptDestroyHash(hHash);
    }

    BCryptCloseAlgorithmProvider(hAlg, 0);
    return TRUE;
}

inline bool verifyLauncherToken() {
    int argcW{};
    wchar_t** argvW = CommandLineToArgvW(GetCommandLineW(), &argcW);
    if (argcW < 2) {
        return false;
    }

    wchar_t* token = argvW[1];
    wchar_t* sep = wcschr(token, L':');
    if (sep == nullptr) {
        return false;
    }

    std::array<wchar_t, 32> tsW = {0};
    lstrcpynW(tsW.data(), token, static_cast<int>(sep - token + 1));
    long long receivedTs = _wtoi64(tsW.data());
    wchar_t* receivedHex = sep + 1;

    std::array<char, 32> secret{};
    security_utils::getSecret(secret.data());

    bool valid{};
    // Kiểm tra cửa sổ thời gian +/- 1 phút để tránh lệch clock
    for (long long t = receivedTs - 1; t <= receivedTs + 1; ++t) {
        std::array<char, 32> testTsA{};
        wsprintfA(testTsA.data(), "%lld", t);

        std::array<BYTE, 32> hash{};
        security_utils::computeHMAC(secret.data(), testTsA.data(), hash.data());

        // wchar_t expectedHex[65] = {0};
        std::array<wchar_t, 64> expectedHex{};
        for (int i = 0; i < 32; ++i) {
            wsprintfW(&expectedHex[static_cast<unsigned long long>(i) * 2],
                      L"%02x",
                      hash[static_cast<unsigned long long>(i)]);
        }

        if (lstrcmpiW(receivedHex, expectedHex.data()) == 0) {
            valid = true;
            break;
        }
    }

    LocalFree(argvW);
    return valid;
}

} // namespace security_utils
