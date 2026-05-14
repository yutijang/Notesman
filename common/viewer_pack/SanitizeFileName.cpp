#include "common/viewer_pack/SanitizeFileName.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace {

bool isWindowsReservedName(std::string_view filename) {
    // Lấy basename (trước dấu '.')
    auto const dotPos = filename.find('.');
    std::string_view base =
        (dotPos == std::string_view::npos) ? filename : filename.substr(0, dotPos);

    if (base.empty()) {
        return false;
    }

    // Uppercase để so sánh không phân biệt hoa thường
    char upper[6] = {};
    if (base.size() >= sizeof(upper)) {
        return false;
    } // COM10 không reserved

    for (std::size_t i = 0; i < base.size(); ++i) {
        upper[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(base[i])));
    }

    std::string_view u(upper);

    // Danh sách reserved
    static constexpr std::string_view reserved[] = {
        "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
        "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

    return std::ranges::any_of(reserved, [u](std::string_view r) {
        return u == r;
    });
}

constexpr std::array<bool, 256> createForbiddenLookup() {
    std::array<bool, 256> lookup = {false};
    // Các mã điều khiển ASCII (0-31)
    for (int i = 0; i < 32; ++i) {
        lookup[i] = true;
    }
    // Ký tự cấm trên Windows/Linux
    for (unsigned char c : {'<', '>', ':', '"', '/', '\\', '|', '?', '*'}) {
        lookup[c] = true;
    }
    return lookup;
}

constexpr auto FORBIDDEN_LOOKUP = createForbiddenLookup();
} // namespace

namespace ViewerPackUltis {
std::string sanitizeFileName(std::string_view input, char replacement) {
    if (input.empty()) {
        return "unnamed";
    }

    std::string result(input);

    // Duyệt một vòng lặp: Thay thế ký tự cấm và khoảng trắng nếu cần
    // Lưu ý: Chúng ta xử lý trên từng byte, an toàn với UTF-8 vì các ký tự cấm
    // đều nằm trong dải ASCII (< 128).
    for (char& c : result) {
        if (static_cast<unsigned char>(c) < 128 &&
            FORBIDDEN_LOOKUP[static_cast<unsigned char>(c)]) {
            c = replacement;
        }
    }

    // Trim dấu chấm và khoảng trắng ở cuối (Đặc thù Windows)
    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
        result.pop_back();
    }

    if (result.empty()) {
        return "unnamed";
    }

    // Xử lý Windows Reserved Names
    if (isWindowsReservedName(result)) {
        result.insert(0, "_");
    }

    // Giới hạn độ dài (255 bytes cho hầu hết các File System hiện đại)
    if (result.length() > 255) {
        result.erase(255);
    }

    return result;
}

} // namespace ViewerPackUltis
