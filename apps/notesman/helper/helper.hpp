#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

enum class ResourceType : std::uint8_t;

enum class IndexableResult : std::uint8_t {
    Yes,
    NoUnsupportedType,
    NoTooLarge,
    NoBinaryDetected,
    NoFileAccess
};

namespace Utils {
    [[nodiscard]] std::string getExtensionFromDownloadUrl(std::string const& url);

    std::string normalizationDBFileSize(std::uint64_t size);

    std::string readFileToString(std::filesystem::path const& path);

    IndexableResult isIndexable(ResourceType type, std::filesystem::path const& path);

    std::string joinTags(std::vector<std::string> const& tags);

    [[nodiscard]] bool looksLikeCppCode(std::string_view text) noexcept;

    void trimS(std::string& source);

    std::string sanitizeFtsQuery(std::string_view input, bool wrapInQuotes = true);

    std::string normalizeSnippet(std::string_view input);

    std::string toLikeQuery(std::string_view input);

    // Lấy phần mở rộng của đường dẫn (vd: "file.txt" -> "txt")
    [[nodiscard]] inline std::string getFileExtension(std::filesystem::path const& path) {
        auto ext = path.extension().string();
        if (!ext.empty() && ext.front() == '.') {
            ext.erase(0, 1); // bỏ dấu '.'
        }
        return ext;
    }

    // Lấy tên file hoặc tên thư mục,
    // trả về rỗng nếu path là thư mục gốc,
    // (vd: "/a/b/file.txt" -> "file.txt")
    [[nodiscard]] inline std::string getDirectoryOrFileName(std::filesystem::path const& path) {
        if (path.has_filename()) { return path.filename().string(); }
        // Nếu kết thúc bằng '/', lấy filename của thư mục cha
        return path.parent_path().filename().string();
    }

    // Lấy đường dẫn tuyệt đối
    [[nodiscard]] inline std::filesystem::path getAbsolutePath(std::filesystem::path const& path) {
        return std::filesystem::absolute(path);
    }

    // Chuẩn hóa đường dẫn (vd: xử lý ../ ./)
    [[nodiscard]] inline std::filesystem::path normalizePath(std::filesystem::path const& path) {
        return std::filesystem::weakly_canonical(path);
    }
} // namespace Utils
