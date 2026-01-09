#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <format>
#include <unordered_map>
#include <vector>
#include <optional>
#include <utility>
#include <filesystem>
#include <sqlite3.h>

#include "helper.hpp"

enum class ResourceType : std::uint8_t {
    unknown,
    plainText, //> text thuần ghi trực tiếp vào database,
               // ghi chú text thường (QTextEdit, có hoặc không highlight)
    cCppCode,  //> text thuần dạng file / snippet / mã nguồn C/C++ (QTextEdit + highlight)
    htmlDoc,   //> .html (WebView)
    pdfDoc,    //> .pdf (PDF viewer)
    epubDoc    //> .epub (Epub viewer)
};

[[nodiscard]] inline const char* resourceTypeToString(ResourceType type) noexcept {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-default"
    switch (type) {
        case ResourceType::plainText: return "text";
        case ResourceType::cCppCode : return "cpp";
        case ResourceType::htmlDoc  : return "html";
        case ResourceType::pdfDoc   : return "pdf";
        case ResourceType::epubDoc  : return "epub";
        case ResourceType::unknown  : break;
    }
#pragma clang diagnostic pop
    std::unreachable(); // compiler hiểu: chỗ này không bao giờ tới
    // Dùng để tối ưu và cảnh báo logic
    //     Khi compiler biết “điểm này không thể tới được”,
    //     nó có thể :
    //     Bỏ sinh mã máy không cần
    //     thiết(optimization hint)
    //         Báo warning nếu có nhánh nào thực tế có thể tới(vì logic sai)
    //             Tránh phải “return fallback” giả(như return {}; hay return 0;)
}

[[nodiscard]] inline ResourceType resourceTypeFromString(std::string_view str) {
    if (str == "text") { return ResourceType::plainText; }
    if (str == "cpp") { return ResourceType::cCppCode; }
    if (str == "html") { return ResourceType::htmlDoc; }
    if (str == "pdf") { return ResourceType::pdfDoc; }
    if (str == "epub") { return ResourceType::epubDoc; }
    throw std::runtime_error(std::format("Unknown ResourceType string: {}", str));
}

[[nodiscard]] inline std::optional<ResourceType> resourceTypeFromExtension(std::string_view ext) {
    const auto &extMap = []() -> const std::unordered_map<std::string_view, ResourceType> & {
        static const auto* map = new std::unordered_map<std::string_view, ResourceType>{
            { "txt", ResourceType::cCppCode},
            {   "c", ResourceType::cCppCode},
            { "cpp", ResourceType::cCppCode},
            {   "h", ResourceType::cCppCode},
            { "hpp", ResourceType::cCppCode},
            { "cxx", ResourceType::cCppCode},
            { "hxx", ResourceType::cCppCode},
            {"html",  ResourceType::htmlDoc},
            { "pdf",   ResourceType::pdfDoc},
            {"epub",  ResourceType::epubDoc}
        };
        return *map;
    }();

    if (auto it = extMap.find(ext); it != extMap.end()) { return it->second; }

    return std::nullopt;
}

[[nodiscard]] inline std::optional<ResourceType>
    resourceTypeFromFile(const std::filesystem::path &pathStr) {
    const auto ext = Utils::getFileExtension(pathStr);
    return resourceTypeFromExtension(ext);
}

struct Resource {
        sqlite3_int64 id{};    //> id của resource
        std::string title;     //> tiêu đề của tài nguyên
        ResourceType type;     //> plainText, cCppCode, htmlDoc, pdfDoc, epubDoc for viewer/behavior
        std::string file_hash; //> hash file (có thể rỗng nếu là text)
        std::string created_at; //> timestamp tạo
        std::string updated_at; //> timestamp cập nhật
};

struct FullResource {
        Resource resource;
        std::optional<std::string> content;
        std::optional<std::string> filepath;
        std::vector<std::string> tags;
};

struct FileEntry {
        sqlite3_int64 resource_id{};
        std::optional<std::string> stored_path;
        std::string original_path;
        bool is_managed{};
};

enum class ResourceFlags : std::uint8_t {
    none = 0,

    // ===== Nguồn khớp tìm kiếm =====
    matchTitle = 1U << 0,   // Khớp từ title
    matchContent = 1U << 1, // Khớp từ nội dung text / file_text
    matchTag = 1U << 2,     // Khớp từ tag

    // ===== Trạng thái nội dung =====
    hasSnippet = 1U << 3,     // Có snippet hợp lệ (FTS snippet)
    hasFullContent = 1U << 4, // Có content đầy đủ trong DB (text_content)

    // ===== Trạng thái file =====
    isFile = 1U << 5,    // Là tài nguyên file
    isManaged = 1U << 6, // File được app quản lý (copied vào storage)
    isExternal = 1U << 7 // File liên kết ngoài (linked)
};

constexpr ResourceFlags operator|(ResourceFlags a, ResourceFlags b) {
    return static_cast<ResourceFlags>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

constexpr ResourceFlags operator&(ResourceFlags a, ResourceFlags b) {
    return static_cast<ResourceFlags>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

constexpr bool hasFlag(ResourceFlags value, ResourceFlags flag) {
    return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0;
}

struct UnifiedSearchResult {
        Resource res;
        std::string displaySubText;
        std::optional<std::string> rawSnippet;
        std::vector<std::string> tags;
        ResourceFlags flags;
};
