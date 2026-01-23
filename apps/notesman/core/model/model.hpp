#pragma once

#include <cstddef>
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
#include <array>
#include <cassert>
#include <sqlite3.h>

#include "helper.hpp"

enum class ResourceType : std::uint8_t {
    unknown,
    plainText, //> text thuần ghi trực tiếp vào database,
               // ghi chú text thường (QTextEdit, có hoặc không highlight)
    markdown,
    cCppCode,  //> text thuần dạng file / snippet / mã nguồn C/C++ (QTextEdit + highlight)
    htmlDoc,   //> .html (WebView)
    pdfDoc,    //> .pdf (PDF viewer)
    epubDoc,   //> .epub (Epub viewer)
    count
};

struct ResourceTypeMeta {
        ResourceType type;
        std::string_view key;
};

inline constexpr std::array<ResourceTypeMeta, static_cast<std::size_t>(ResourceType::count) - 1>
    K_RESOURCE_TYPE_TABLE{
        {{.type = ResourceType::plainText, .key = "text"},
         {.type = ResourceType::cCppCode, .key = "cpp"},
         {.type = ResourceType::markdown, .key = "markdown"},
         {.type = ResourceType::htmlDoc, .key = "html"},
         {.type = ResourceType::pdfDoc, .key = "pdf"},
         {.type = ResourceType::epubDoc, .key = "epub"}}
};

inline const std::unordered_map<std::string_view, ResourceType> K_EXT_MAP{
    // Plain text
    {     "txt", ResourceType::plainText},
    {    "text", ResourceType::plainText},
    {     "log", ResourceType::plainText},
    {     "ini", ResourceType::plainText},
    {     "cfg", ResourceType::plainText},
    {    "conf", ResourceType::plainText},
    {     "csv", ResourceType::plainText},

    // Markdown (parse → html)
    {      "md",  ResourceType::markdown},
    {"markdown",  ResourceType::markdown},
    {     "mdx",  ResourceType::markdown},

    // C / C++
    {       "c",  ResourceType::cCppCode},
    {     "cpp",  ResourceType::cCppCode},
    {      "cc",  ResourceType::cCppCode},
    {     "cxx",  ResourceType::cCppCode},
    {       "h",  ResourceType::cCppCode},
    {      "hh",  ResourceType::cCppCode},
    {     "hpp",  ResourceType::cCppCode},
    {     "hxx",  ResourceType::cCppCode},

    // HTML
    {    "html",   ResourceType::htmlDoc},
    {     "htm",   ResourceType::htmlDoc},
    {   "xhtml",   ResourceType::htmlDoc},
    {   "shtml",   ResourceType::htmlDoc},

    // Binary documents
    {     "pdf",    ResourceType::pdfDoc},
    {    "epub",   ResourceType::epubDoc}
};

[[nodiscard]] inline std::string_view resourceTypeToString(ResourceType type) noexcept {
    for (const auto &m : K_RESOURCE_TYPE_TABLE) {
        if (m.type == type) { return m.key; }
    }
    assert(false && "Invalid ResourceType");
    std::unreachable();
}

[[nodiscard]] inline ResourceType resourceTypeFromString(std::string_view str) {
    for (const auto &m : K_RESOURCE_TYPE_TABLE) {
        if (m.key == str) { return m.type; }
    }
    throw std::runtime_error(std::format("Unknown ResourceType: {}", str));
}

[[nodiscard]] inline std::optional<ResourceType> resourceTypeFromExtension(std::string_view ext) {
    if (auto it = K_EXT_MAP.find(ext); it != K_EXT_MAP.end()) { return it->second; }
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

// Bitmask
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

constexpr ResourceFlags operator~(ResourceFlags v) {
    return static_cast<ResourceFlags>(~static_cast<std::uint8_t>(v));
}

constexpr ResourceFlags &operator|=(ResourceFlags &lhs, ResourceFlags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool hasFlag(ResourceFlags value, ResourceFlags flag) {
    return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0;
}

struct UnifiedSearchResult {
        Resource res;
        std::string displaySubText;
        std::optional<std::string> rawSnippet;
        std::optional<std::string> filePath;
        std::vector<std::string> tags;
        ResourceFlags flags;
};
