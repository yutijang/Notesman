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
    ResourceType type;
    std::string_view key;
};

inline constexpr std::array<ResourceTypeMeta, static_cast<std::size_t>(ResourceType::Count) - 1>
    K_RESOURCE_TYPE_TABLE{
        {{.type = ResourceType::PlainText, .key = "text"},
         {.type = ResourceType::CCppCode, .key = "cpp"},
         {.type = ResourceType::Markdown, .key = "markdown"},
         {.type = ResourceType::HtmlDoc, .key = "html"},
         {.type = ResourceType::PdfDoc, .key = "pdf"},
         {.type = ResourceType::EpubDoc, .key = "epub"},
         {.type = ResourceType::Url, .key = "url"}}
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
    sqlite3_int64 id{};     //> id của resource
    std::string uuid;

    std::string title;      //> tiêu đề của tài nguyên
    ResourceType type;      //> plainText, cCppCode, htmlDoc, pdfDoc, epubDoc for viewer/behavior
    std::string file_hash;  //> hash file (có thể rỗng nếu là text)
                            //
    std::string created_at; //> timestamp tạo
    std::string updated_at; //> timestamp cập nhật
};

struct FullResource {
    Resource resource;
    std::optional<std::string> content;
    std::optional<std::string> filepath;
    std::optional<std::string> url;
    std::vector<std::string> tags;
};

struct FileEntry {
    sqlite3_int64 resource_id{};
    std::optional<std::string> stored_path;
    std::string original_path;
    bool is_managed{};
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
