#pragma once

#include <filesystem>
#include <ranges>
#include <string>
#include <string_view>
#include <cstdint>

enum class ResourceType : std::uint8_t;

enum class IndexableResult : std::uint8_t {
    yes,
    noUnsupportedType,
    noTooLarge,
    noBinaryDetected,
    noFileAccess
};

namespace Utils {
    [[nodiscard]] std::string getExtensionFromDownloadUrl(const std::string &url);

    std::string normalizationDBFileSize(std::uint64_t size);

    std::string readFileToString(const std::filesystem::path &path);

    IndexableResult isIndexable(ResourceType type, const std::filesystem::path &path);

    // Lấy phần mở rộng của đường dẫn (vd: "file.txt" -> "txt")
    [[nodiscard]] inline std::string getFileExtension(const std::filesystem::path &path) {
        auto ext = path.extension().string();
        if (!ext.empty() && ext.front() == '.') {
            ext.erase(0, 1); // bỏ dấu '.'
        }
        return ext;
    }

    // Lấy tên file hoặc tên thư mục,
    // trả về rỗng nếu path là thư mục gốc,
    // (vd: "/a/b/file.txt" -> "file.txt")
    [[nodiscard]] inline std::string getDirectoryOrFileName(const std::filesystem::path &path) {
        if (path.has_filename()) { return path.filename().string(); }
        // Nếu kết thúc bằng '/', lấy filename của thư mục cha
        return path.parent_path().filename().string();
    }

    // Lấy đường dẫn tuyệt đối
    [[nodiscard]] inline std::filesystem::path getAbsolutePath(const std::filesystem::path &path) {
        return std::filesystem::absolute(path);
    }

    // Chuẩn hóa đường dẫn (vd: xử lý ../ ./)
    [[nodiscard]] inline std::filesystem::path normalizePath(const std::filesystem::path &path) {
        return std::filesystem::weakly_canonical(path);
    }

    static std::string sanitizeFtsQuery(std::string_view input, bool wrapInQuotes = true) {
        if (input.empty()) { return {}; }

        std::string clean{input};
        bool hasWildcard{};

        // 1. Kiểm tra dấu *
        if (clean.back() == '*') {
            hasWildcard = true;
            clean.pop_back();
        }

        // 2. Xóa nháy kép
        std::erase(clean, '\"');
        if (clean.empty()) { return {}; }

        // 3. Đóng gói
        if (wrapInQuotes) {
            // Mode Title: Bọc nháy kép toàn bộ để tìm chính xác cụm từ
            return "\"" + clean + (hasWildcard ? "*\"" : "\"");
        }

        // Mode Content: Không bọc nháy kép để tìm linh hoạt (từng từ rời rạc)
        // Nhưng vẫn thêm * nếu người dùng yêu cầu
        return clean + (hasWildcard ? "*" : "");
    }

    static std::string toLikeQuery(std::string_view input) {
        if (input.empty()) { return {}; }
        std::string clean{input};
        std::erase(clean, '\"');

        auto isSpace = [](unsigned char ch) { return std::isspace(ch); };
        auto trimmedView = clean | std::views::drop_while(isSpace) | std::views::reverse |
                           std::views::drop_while(isSpace) | std::views::reverse;

        std::string trimmedS(trimmedView.begin(), trimmedView.end());

        return trimmedS + "%"; // Tìm kiếm theo kiểu "bắt đầu bằng"
    }

    // NOLINTBEGIN (readability-magic-numbers)
    [[nodiscard]] inline bool looksLikeCppCode(std::string_view text) noexcept {
        if (text.size() < 30) { return false; }

        int score{};

        if (text.contains("#include <") || text.contains("#include \"")) {
            score += 3;
            if (score >= 5) { return true; }
        }
        if (text.contains("namespace ") || text.contains("class ") || text.contains("struct ")) {
            score += 3;
            if (score >= 5) { return true; }
        }
        if (text.contains("::")) {
            score += 2;
            if (score >= 5) { return true; }
        }
        if (text.contains("->")) {
            score += 1;
            if (score >= 5) { return true; }
        }

        return score >= 5;
    }

    // NOLINTEND
} // namespace Utils
