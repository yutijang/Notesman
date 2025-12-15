#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Utils {
    // Lấy phần mở rộng của đường dẫn (vd: "file.txt" -> "txt")
    [[nodiscard]] inline std::string getFileExtension(const std::filesystem::path &path) {
        auto ext = path.extension().string();
        if (!ext.empty() && ext.front() == '.') {
            ext.erase(0, 1); // bỏ dấu '.'
        }
        return ext;
    }

    // Lấy tên file (vd: "/a/b/file.txt" -> "file.txt")
    [[nodiscard]] inline std::string getFileName(const std::filesystem::path &path) {
        return path.filename().string();
    }

    // Lấy đường dẫn tuyệt đối
    [[nodiscard]] inline std::filesystem::path getAbsolutePath(const std::filesystem::path &path) {
        return std::filesystem::absolute(path);
    }

    // Chuẩn hóa đường dẫn (vd: xử lý ../ ./)
    [[nodiscard]] inline std::filesystem::path normalizePath(const std::filesystem::path &path) {
        return std::filesystem::weakly_canonical(path);
    }

    [[nodiscard]] std::string getExtensionFromDownloadUrl(const std::string &url);

    [[nodiscard]] inline bool looksLikeCppCode(std::string_view text) noexcept {
        if (text.size() < 30) { return false; } // NOLINT(readability-magic-numbers)

        int score{};

        if (text.contains("#include <") || text.contains("#include \"")) { score += 3; }
        if (text.contains("class ") || text.contains("struct ") || text.contains("namespace ")) {
            score += 3;
        }
        if (text.contains("::")) { score += 2; }
        if (text.contains("std::")) { score += 2; }
        if (text.contains("->")) { score += 1; }

        return score >= 5; // NOLINT(readability-magic-numbers)
    }
} // namespace Utils
