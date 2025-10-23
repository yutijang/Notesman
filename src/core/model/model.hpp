#pragma once

#include "helper.hpp"
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

enum class ResourceType : std::uint8_t { text, cpp, pdf, epub };

[[nodiscard]] inline const char* resourceTypeToString(ResourceType type) noexcept {
    switch (type) {
        case ResourceType::text: return "text";
        case ResourceType::cpp : return "cpp";
        case ResourceType::pdf : return "pdf";
        case ResourceType::epub: return "epub";
    }
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
    if (str == "text") { return ResourceType::text; }
    if (str == "cpp") { return ResourceType::cpp; }
    if (str == "pdf") { return ResourceType::pdf; }
    if (str == "epub") { return ResourceType::epub; }
    throw std::runtime_error(std::format("Unknown ResourceType string: {}", str));
}

[[nodiscard]] inline std::optional<ResourceType> resourceTypeFromExtension(std::string_view ext) {
    static const std::unordered_map<std::string_view, ResourceType> extMap = {
        { "txt", ResourceType::text},
        { "cpp",  ResourceType::cpp},
        {   "h",  ResourceType::cpp},
        { "pdf",  ResourceType::pdf},
        {"epub", ResourceType::epub}
    };

    // Cú pháp tìm kiếm ngắn gọn hơn (if-with-initializer & structured binding)
    if (auto it = extMap.find(ext); it != extMap.end()) { return it->second; }

    return std::nullopt;
}

[[nodiscard]] inline std::optional<ResourceType> resourceTypeFromFile(const std::string &pathStr) {
    const std::filesystem::path path(pathStr);
    const auto ext = Utils::getFileExtension(path);
    return resourceTypeFromExtension(ext);
}

struct Resource {
        sqlite3_int64 id{};     // id của resource
        std::string title;      // tiêu đề
        ResourceType type;      // loại: text, cpp, pdf, epub
        std::string file_hash;  // hash file (có thể rỗng nếu là text)
        std::string created_at; // timestamp tạo
        std::string updated_at; // timestamp cập nhật
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
