#include <string>
#include <stdexcept>
#include <optional>
#include <filesystem>
#include <sqlite3.h>
#include "file_repository.hpp"
#include "model.hpp"
#include "resource_service.hpp"
#include "file_service.hpp"
#include "tag_repository.hpp"
#include "text_content_repository.hpp"
#include "resource_repository.hpp"

// NOLINTNEXTLINE
sqlite3_int64 ResourceService::addTextResource(const std::string &title, const std::string &content,
                                               ResourceType type) {
    if (type != ResourceType::text) {
        throw std::runtime_error("addTextResource only supports ResourceType::text");
    }

    // Insert vào resources (file_hash để trống)
    sqlite3_int64 resourceId =
        m_resRepo.insert({.title = title, .type = type, .file_hash = ""}); // NOLINT

    // Insert nội dung text vào text_content
    m_textRepo.insertText(resourceId, content);

    return resourceId;
}

sqlite3_int64 ResourceService::addFileResource(const std::string &filepath,
                                               const std::string &title, ResourceType type,
                                               bool isManaged) {
    return m_fileService.addFileResource(filepath, title, type, isManaged);
}

std::optional<FullResource> ResourceService::getFullResource(sqlite3_int64 resourceId) {
    // Lấy resource gốc
    auto resOpt = m_resRepo.getById(resourceId);
    if (!resOpt.has_value()) { return std::nullopt; }

    // Lấy tag (có thể rỗng)
    auto tagPairs = m_tagRepo.getTagsByResourceId(resourceId);
    std::vector<std::string> tagNames;
    tagNames.reserve(tagPairs.size());
    for (const auto &p : tagPairs) { tagNames.push_back(p.second); }

    FullResource fres;
    fres.resource = *resOpt;
    fres.tags = std::move(tagNames);

    // Nếu tài nguyên là text
    if (fres.resource.type == ResourceType::text) {
        fres.content = m_textRepo.getTextById(resourceId);
        fres.filepath = std::nullopt;

        return fres;
    }

    // Nếu là file: lấy FileEntry (có thể không tồn tại => trả nullopt)
    auto entryOpt = m_fileRepo.getFileById(resourceId);
    if (!entryOpt.has_value()) { return std::nullopt; }

    // Ưu tiên stored_path nếu có, fallback sang original_path
    if (entryOpt->stored_path.has_value()) {
        fres.filepath = entryOpt->stored_path;
    } else {
        fres.filepath = entryOpt->original_path; // original_path luôn tồn tại theo schema
    }

    fres.content = std::nullopt;                 // file không có text_content trong DB

    return fres;
}

void ResourceService::deleteResource(sqlite3_int64 resourceId) {
    auto fileEntry = m_fileRepo.getFileById(resourceId);

    if (fileEntry.has_value() && fileEntry->is_managed) {
        std::filesystem::remove(*(fileEntry->stored_path));
    }

    m_resRepo.remove(resourceId);
}

std::vector<Resource> ResourceService::searchByTitle(const std::string &keyword) {
    return m_resRepo.searchByTitleFTS(keyword);
}

std::vector<FullResource> ResourceService::searchByTitleFull(const std::string &keyword) {
    std::vector<FullResource> results;
    auto matches = m_resRepo.searchByTitleFTS(keyword);
    results.reserve(matches.size());

    for (const auto &res : matches) {
        auto full = getFullResource(res.id);
        if (full.has_value()) { results.push_back(*full); }
    }

    return results;
}

std::vector<std::pair<sqlite3_int64, std::string>>
    ResourceService::searchByContent(const std::string &keyword) {
    return m_textRepo.searchByContentFTS(keyword);
}

std::vector<FullResource> ResourceService::searchByContentFull(const std::string &keyword) {
    std::vector<FullResource> results;
    auto matches = m_textRepo.searchByContentFTS(keyword);
    results.reserve(matches.size());

    for (auto &[resourceId, snippet] : matches) {
        auto full = getFullResource(resourceId);
        if (full.has_value()) {
            // override content bằng snippet highlight
            full->content = snippet;
            results.push_back(std::move(*full));
        }
    }
    return results;
}

std::vector<Resource> ResourceService::getResourcesByTags(const std::vector<std::string> &tags) {
    return m_tagRepo.getResourcesViaTags(tags);
}

void ResourceService::addTagToResource(sqlite3_int64 resourceId, const std::string &tag) {
    sqlite3_int64 tagId{};
    auto tagIdOpt = m_tagRepo.getTagIdByName(tag);

    // Lấy tagId nếu đã tồn tại
    if (tagIdOpt.has_value()) {
        tagId = *tagIdOpt;
    } else {
        // Nếu chưa có → thêm tag mới
        auto newTagIdOpt = m_tagRepo.addTag(tag);
        if (!newTagIdOpt.has_value()) {
            throw std::runtime_error("Failed to insert new tag: " + tag);
        }

        tagId = *newTagIdOpt;
    }

    m_tagRepo.linkResourceIdWithTag({.resourceId = resourceId, .tagId = tagId});
}

void ResourceService::addTagsToResource(sqlite3_int64 resourceId,
                                        const std::vector<std::string> &tagNames) {
    m_tagRepo.linkResourceWithTags(resourceId, tagNames);
}

void ResourceService::removeTagFromResource(sqlite3_int64 resourceId, const std::string &tag) {
    auto tagIdOpt = m_tagRepo.getTagIdByName(tag);
    if (!tagIdOpt.has_value()) { return; }

    m_tagRepo.deleteTagFromResource({.resourceId = resourceId, .tagId = *tagIdOpt});
}

std::vector<std::pair<sqlite3_int64, std::string>> ResourceService::getAllTags() {
    return m_tagRepo.getAllTags();
}

std::vector<Resource> ResourceService::getResourcesByTag(const std::string &tag) {
    return m_tagRepo.getResourcesViaOneTag(tag);
}

std::vector<FullResource> ResourceService::getFullResourcesByTag(const std::string &tag) {
    std::vector<FullResource> results;
    auto resources = m_tagRepo.getResourcesViaOneTag(tag);
    results.reserve(resources.size());

    for (const auto &res : resources) {
        auto full = getFullResource(res.id);
        if (full.has_value()) { results.push_back(std::move(*full)); }
    }

    return results;
}

bool ResourceService::isExistTitle(std::string_view title, ResourceType type) const {
    return m_resRepo.existsTitle(title, type);
}