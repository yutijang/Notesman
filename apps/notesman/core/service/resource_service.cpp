#include <string>
#include <string_view>
#include <stdexcept>
#include <optional>
#include <filesystem>
#include <sqlite3.h>
#include <utility>
#include <vector>

#include "model.hpp"
#include "resource_repository.hpp"
#include "file_repository.hpp"
#include "tag_repository.hpp"
#include "text_content_repository.hpp"
#include "resource_service.hpp"
#include "file_service.hpp"
#include "Logger.hpp"

// NOLINTNEXTLINE
sqlite3_int64 ResourceService::addTextResource(const std::string &title, const std::string &content,
                                               ResourceType type) {
    if (type != ResourceType::plainText) {
        std::string msg{"addTextResource only supports ResourceType::plainText"};
        Log::err(msg);
        throw std::runtime_error(msg);
    }

    // Insert vào resources (file_hash để trống)
    sqlite3_int64 resourceId =
        m_resRepo.insert({.title = title,
                          .type = type,
                          .file_hash = ""}); // NOLINT (-Wmissing-designated-field-initializers)

    // Insert nội dung text vào text_content
    m_textRepo.insertText(resourceId, content);

    return resourceId;
}

sqlite3_int64 ResourceService::addFileResource(const std::string &filepath,
                                               const std::string &title, ResourceType type,
                                               bool isManaged, const std::string &contentToIndex) {
    return m_fileService.addFileResource(filepath, title, type, isManaged, contentToIndex);
}

std::optional<FullResource> ResourceService::getFullResource(sqlite3_int64 resourceId,
                                                             bool includeContent) {
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

    // Nếu tài nguyên là text thuần
    if (fres.resource.type == ResourceType::plainText) {
        if (includeContent) {
            fres.content = m_textRepo.getTextById(resourceId);
        } else {
            fres.content = std::nullopt; // Để trống để chờ gán snippet ở Service Search
        }

        fres.filepath = std::nullopt;
    } else { // Nếu là file: lấy FileEntry (có thể không tồn tại => trả nullopt)
        auto entryOpt = m_fileRepo.getFileById(resourceId);
        if (!entryOpt.has_value()) { return std::nullopt; }

        // Ưu tiên stored_path nếu có, fallback sang original_path
        if (entryOpt->stored_path.has_value()) {
            fres.filepath = entryOpt->stored_path;
        } else {
            fres.filepath = entryOpt->original_path; // original_path luôn tồn tại theo schema
        }

        fres.content = std::nullopt;                 // Luôn để trống, snippet sẽ được gán sau
    }

    return fres;
}

void ResourceService::deleteResource(sqlite3_int64 resourceId) {
    auto fileEntry = m_fileRepo.getFileById(resourceId);

    if (fileEntry.has_value() && fileEntry->is_managed) {
        std::filesystem::remove(*(fileEntry->stored_path));
    }

    m_resRepo.remove(resourceId);
}

void ResourceService::deleteResources(const std::vector<sqlite3_int64> &resourceIds) {
    if (resourceIds.empty()) { return; }

    for (const auto id : resourceIds) {
        auto fileEntry = m_fileRepo.getFileById(id);
        if (fileEntry.has_value() && fileEntry->is_managed) {
            try {
                std::filesystem::remove(*(fileEntry->stored_path));
            } catch (const std::filesystem::filesystem_error &e) {
                Log::warn("Failed to delete file: {}", e.what());
                throw std::runtime_error("[WARN] Failed to delete file: " +
                                         std::string{(e.what())});
            }
        }
    }

    m_resRepo.removeBatch(resourceIds);
}

std::vector<UnifiedSearchResult> ResourceService::searchByTitle(const std::string &keyword) {
    return m_resRepo.searchByTitleFTS(keyword);
}

std::vector<UnifiedSearchResult> ResourceService::searchByTitleFull(const std::string &keyword) {
    return m_resRepo.searchByTitleFTS(keyword);
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

std::vector<UnifiedSearchResult>
    ResourceService::searchByContentUnifiedFull(const std::string &keyword) {
    auto results = m_resRepo.searchByContentUnified(keyword);

    for (auto &item : results) { item.displaySubText = Utils::normalizeSnippet(*item.rawSnippet); }

    return results;
}

std::vector<UnifiedSearchResult> ResourceService::searchUnifiedFull(std::string_view likeKW,
                                                                    std::string_view ftsKW) {
    auto results = m_resRepo.searchUnified(likeKW, ftsKW);

    for (auto &item : results) {
        if (hasFlag(item.flags, ResourceFlags::matchContent) && item.rawSnippet.has_value()) {
            item.displaySubText = Utils::normalizeSnippet(*item.rawSnippet);
        } else if (hasFlag(item.flags, ResourceFlags::matchTag)) {
            std::vector<std::string> tagNames;
            auto tags = m_tagRepo.getTagsByResourceId(item.res.id);
            tagNames.reserve(tags.size());
            for (const auto &[id, name] : tags) { tagNames.push_back(name); }
            item.tags = tagNames;
            item.displaySubText = Utils::joinTags(item.tags);
        } else if (hasFlag(item.flags, ResourceFlags::matchTitle)) {
            item.displaySubText = item.res.updated_at;
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
            Log::err("Failed to insert new tag: {}", tag);
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

FullResource ResourceService::buildFullFromResource(const Resource &res) {
    FullResource fr;

    fr.resource = res;
    fr.content = m_textRepo.getTextById(res.id);
    fr.filepath = m_fileRepo.getFilepathByResourceId(res.id);

    auto tagPairs = m_tagRepo.getTagsByResourceId(res.id); // vector<pair<id,name>>
    fr.tags.reserve(tagPairs.size());

    for (auto &p : tagPairs) { fr.tags.push_back(p.second); }

    return fr;
}

std::vector<FullResource> ResourceService::getAllFull() {
    std::vector<FullResource> out;

    auto resources = m_resRepo.getAll();
    if (!resources.empty()) {
        out.reserve(resources.size());
        for (auto &r : resources) { out.push_back(buildFullFromResource(r)); }
    }

    return out;
}

std::vector<UnifiedSearchResult> ResourceService::getAllUnified() {
    std::vector<UnifiedSearchResult> out;

    auto resources = m_resRepo.getAll();
    out.reserve(resources.size());

    for (const auto &r : resources) {
        UnifiedSearchResult u{};
        u.res = r;

        // SubText: ngày cập nhật
        if (!r.updated_at.empty()) {
            u.displaySubText = r.updated_at;
        } else {
            u.displaySubText = r.created_at;
        }

        // Tags
        auto tagPairs = m_tagRepo.getTagsByResourceId(r.id);
        for (auto &p : tagPairs) { u.tags.push_back(p.second); }

        u.flags = ResourceFlags::none;
        out.push_back(std::move(u));
    }

    return out;
}

void ResourceService::updateText(sqlite3_int64 resourceId, std::string_view newText) {
    m_textRepo.updateText(resourceId, newText);
}
