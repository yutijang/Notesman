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

    // Lấy tên file hoặc tên thư mục
    // trả về rỗng nếu path là thư mục gốc
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

    [[nodiscard]] std::string getExtensionFromDownloadUrl(const std::string &url);

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
