#pragma once

#include "helper/helper.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

enum class ResourceType : std::uint8_t {
    Unknown,
    PlainText, //> text thuần ghi trực tiếp vào database,
               // ghi chú text thường (QTextEdit, có hoặc không highlight)
    CCppCode,  //> text thuần dạng file / snippet / mã nguồn C/C++ (QTextEdit + highlight)
    Markdown,
    HtmlDoc,   //> .html (WebView)
    PdfDoc,    //> .pdf (PDF viewer)
    EpubDoc,   //> .epub (Epub viewer)
    Url,
    Count
};

struct ResourceTypeMeta {
    std::string_view key; // size: 16, align: 8, offset: 0
    ResourceType type;    // size: 1, align: 1, offset: 16
                          // +7 tail padding
};

inline constexpr std::array<ResourceTypeMeta, static_cast<std::size_t>(ResourceType::Count) - 1>
    K_RESOURCE_TYPE_TABLE{
        {{.key = "text", .type = ResourceType::PlainText},
         {.key = "cpp", .type = ResourceType::CCppCode},
         {.key = "markdown", .type = ResourceType::Markdown},
         {.key = "html", .type = ResourceType::HtmlDoc},
         {.key = "pdf", .type = ResourceType::PdfDoc},
         {.key = "epub", .type = ResourceType::EpubDoc},
         {.key = "url", .type = ResourceType::Url}}
};

inline std::unordered_map<std::string_view, ResourceType> const K_EXT_MAP{
    // Plain text
    {     "txt", ResourceType::PlainText},
    {    "text", ResourceType::PlainText},
    {     "log", ResourceType::PlainText},
    {     "ini", ResourceType::PlainText},
    {     "cfg", ResourceType::PlainText},
    {    "conf", ResourceType::PlainText},
    {     "csv", ResourceType::PlainText},

    // Markdown (parse → html)
    {      "md",  ResourceType::Markdown},
    {"markdown",  ResourceType::Markdown},
    {     "mdx",  ResourceType::Markdown},

    // C / C++
    {       "c",  ResourceType::CCppCode},
    {     "cpp",  ResourceType::CCppCode},
    {      "cc",  ResourceType::CCppCode},
    {     "cxx",  ResourceType::CCppCode},
    {       "h",  ResourceType::CCppCode},
    {      "hh",  ResourceType::CCppCode},
    {     "hpp",  ResourceType::CCppCode},
    {     "hxx",  ResourceType::CCppCode},

    // HTML
    {    "html",   ResourceType::HtmlDoc},
    {     "htm",   ResourceType::HtmlDoc},
    {   "xhtml",   ResourceType::HtmlDoc},
    {   "shtml",   ResourceType::HtmlDoc},

    // Binary documents
    {     "pdf",    ResourceType::PdfDoc},
    {    "epub",   ResourceType::EpubDoc}
};

[[nodiscard]] inline std::string_view resourceTypeToString(ResourceType type) noexcept {
    for (auto const& m : K_RESOURCE_TYPE_TABLE) {
        if (m.type == type) {
            return m.key;
        }
    }
    assert(false && "Invalid ResourceType");
    std::unreachable();
}

[[nodiscard]] inline ResourceType resourceTypeFromString(std::string_view str) {
    for (auto const& m : K_RESOURCE_TYPE_TABLE) {
        if (m.key == str) {
            return m.type;
        }
    }
    throw std::runtime_error(std::format("Unknown ResourceType: {}", str));
}

[[nodiscard]] inline std::optional<ResourceType> resourceTypeFromExtension(std::string_view ext) {
    if (auto it = K_EXT_MAP.find(ext); it != K_EXT_MAP.end()) {
        return it->second;
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<ResourceType>
    resourceTypeFromFile(std::filesystem::path const& pathStr) {
    auto const ext = Utils::getFileExtension(pathStr);
    return resourceTypeFromExtension(ext);
}

struct Resource {
    // định danh xác minh cho packer
    std::string uuid; // size: 24, align: 8, offset: 0
    // tiêu đề của tài nguyên
    std::string title; // size: 24, align: 8, offset: 24
    // hash file (có thể rỗng nếu là text)
    std::string file_hash; // size: 24, align: 8, offset: 48
    // timestamp tạo
    std::string created_at; // size: 24, align: 8, offset: 72
    // timestamp cập nhật
    std::string updated_at; // size: 24, align: 8, offset: 96
    // id của resource
    sqlite3_int64 id{}; // size: 8, align: 8, offset: 120
    // plainText, cCppCode, htmlDoc, pdfDoc, epubDoc for viewer/behavior
    ResourceType type; // size: 1, align: 1, offset: 128
};

struct FullResource {
    Resource resource;
    std::optional<std::string> content;
    std::optional<std::string> filepath;
    std::optional<std::string> url;
    std::vector<std::string> tags;
};

struct FileEntry {
    sqlite3_int64 resource_id{};            // size: 8, offset: 0
    std::optional<std::string> stored_path; // size: 32, offset: 8
    std::string original_path;              // size: 24, offset:40
    bool is_managed{};                      // size: 1, offset:64
};

struct UrlEntry {
    sqlite3_int64 resource_id{}; // id của resource
    std::string url;             // raw url người dùng nhập
    std::string normalized_url;  // canonical form
    std::string domain;          // ví dụ: w3schools.com
};

using RFBits = std::uint16_t;    // ResourceFlagBits

// Bitmask
enum class ResourceFlags : RFBits {
    None = 0,

    // ===== Nguồn khớp tìm kiếm =====
    MatchTag = 1U << 0,      // 1
    MatchTitle = 1U << 1,    // 2
    MatchText = 1U << 2,     // 4  text_content_fts
    MatchFileText = 1U << 3, // 8  file_text_content_fts
    MatchDomain = 1U << 4,   // 16 resource_urls.domain
    MatchUrlPath = 1U << 5,

    // ===== Trạng thái nội dung =====
    HasSnippet = 1U << 6,     // Có snippet hợp lệ (FTS snippet)
    HasFullContent = 1U << 7, // Có content đầy đủ trong DB (text_content)

    // ===== Trạng thái file =====
    IsFile = 1U << 8,     // Là tài nguyên file
    IsManaged = 1U << 9,  // File được app quản lý (copied vào storage)
    IsExternal = 1U << 10 // File liên kết ngoài (linked)
};

constexpr ResourceFlags operator|(ResourceFlags a, ResourceFlags b) {
    return static_cast<ResourceFlags>(static_cast<RFBits>(a) | static_cast<RFBits>(b));
}

constexpr ResourceFlags operator&(ResourceFlags a, ResourceFlags b) {
    return static_cast<ResourceFlags>(static_cast<RFBits>(a) & static_cast<RFBits>(b));
}

constexpr ResourceFlags operator~(ResourceFlags v) {
    return static_cast<ResourceFlags>(~static_cast<RFBits>(v));
}

constexpr ResourceFlags& operator|=(ResourceFlags& lhs, ResourceFlags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool hasFlag(ResourceFlags value, ResourceFlags flag) noexcept {
    return (static_cast<RFBits>(value) & static_cast<RFBits>(flag)) != 0;
}

constexpr bool hasAnyFlags(ResourceFlags value, ResourceFlags flags) noexcept {
    return (static_cast<RFBits>(value) & static_cast<RFBits>(flags)) != 0;
}

struct UnifiedSearchResult {
    Resource res;
    std::string displaySubText;
    std::optional<std::string> rawSnippet;
    std::optional<std::string> filePath;
    std::optional<std::string> url;
    std::vector<std::string> tags;
    ResourceFlags flags;
};
