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

struct UnifiedSearchResult {
        Resource res;
        std::string snippet;
};
