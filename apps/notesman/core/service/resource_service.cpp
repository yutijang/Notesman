#include "core/service/resource_service.hpp"

#include "common/logger/Logger.hpp"
#include "core/model/model.hpp"
#include "core/repository/file_repository.hpp"
#include "core/repository/resource_repository.hpp"
#include "core/repository/tag_repository.hpp"
#include "core/repository/text_content_repository.hpp"
#include "core/service/file_service.hpp"
#include "helper/helper.hpp"

#include <cassert>
#include <filesystem>
#include <optional>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
sqlite3_int64 ResourceService::addTextResource(std::string const& title,
                                               std::string const& content,
                                               ResourceType type) {
    if (type != ResourceType::PlainText) [[unlikely]] {
        std::string msg{"addTextResource only supports ResourceType::plainText"};
        Log::err(msg);
        throw std::runtime_error(msg);
    }

    // Insert vào resources (file_hash để trống)
    sqlite3_int64 resourceId = m_resRepo.insert({.uuid = {},
                                                 .title = title,
                                                 .type = type,
                                                 .file_hash = {},
                                                 .created_at = {},
                                                 .updated_at = {}});

    // Insert nội dung text vào text_content
    m_textRepo.insertText(resourceId, content);

    return resourceId;
}

sqlite3_int64 ResourceService::addFileResource(std::string const& filepath,
                                               std::string const& title,
                                               ResourceType type,
                                               bool isManaged,
                                               std::string const& contentToIndex) {
    return m_fileService.addFileResource(filepath, title, type, isManaged, contentToIndex);
}

std::optional<sqlite3_int64> ResourceService::addUrlResource(std::string_view title,
                                                             ResourceType type,
                                                             std::string_view rawUrl) {
    return m_urlService.addUrlResource(title, type, rawUrl);
}

std::optional<FullResource> ResourceService::getFullResource(sqlite3_int64 resourceId,
                                                             bool includeContent) {
    // Lấy resource gốc
    auto resOpt = m_resRepo.getById(resourceId);
    if (!resOpt.has_value()) [[unlikely]] {
        return std::nullopt;
    }

    // Lấy tag (có thể rỗng)
    auto tagPairs = m_tagRepo.getTagsByResourceId(resourceId);
    std::vector<std::string> tagNames;
    tagNames.reserve(tagPairs.size());
    for (auto const& p : tagPairs) {
        tagNames.push_back(p.second);
    }

    FullResource fres;
    fres.resource = *resOpt;
    fres.tags = std::move(tagNames);

    if (isExistFile(resourceId)) {
        fres.filepath = m_fileRepo.getFileById(resourceId)->stored_path;
    } else {
        fres.filepath = std::nullopt;
    }

    if (includeContent) {
        fres.content = m_textRepo.getTextById(resourceId);
    }

    fres.url = getUrlByResourceIdOnly(resourceId);

    return fres;
}

void ResourceService::deleteResource(sqlite3_int64 resourceId) {
    auto fileEntry = m_fileRepo.getFileById(resourceId);

    if (fileEntry.has_value() && fileEntry->is_managed) {
        std::filesystem::remove(*(fileEntry->stored_path));
    }

    m_resRepo.remove(resourceId);
}

void ResourceService::deleteResources(std::vector<sqlite3_int64> const& resourceIds) {
    if (resourceIds.empty()) {
        return;
    }

    for (auto const id : resourceIds) {
        auto fileEntry = m_fileRepo.getFileById(id);
        if (fileEntry.has_value() && fileEntry->is_managed) {
            try {
                std::filesystem::remove(*(fileEntry->stored_path));
            } catch (std::filesystem::filesystem_error const& e) {
                Log::warn("Failed to delete file: {}", e.what());
                throw std::runtime_error("[WARN] Failed to delete file: " +
                                         std::string{(e.what())});
            }
        }
    }

    m_resRepo.removeBatch(resourceIds);
}

std::vector<UnifiedSearchResult> ResourceService::searchByTitle(std::string const& keyword) {
    return m_resRepo.searchByTitleFTS(keyword);
}

std::vector<UnifiedSearchResult> ResourceService::searchByTitleFull(std::string const& keyword) {
    auto results = m_resRepo.searchByTitleFTS(keyword);

    for (auto& item : results) {
        validateIsFile(item);
    }

    return results;
}

std::vector<std::pair<sqlite3_int64, std::string>>
    ResourceService::searchByContent(std::string const& keyword) {
    return m_textRepo.searchByContentFTS(keyword);
}

std::vector<FullResource> ResourceService::searchByContentFull(std::string const& keyword) {
    std::vector<FullResource> results;
    auto matches = m_textRepo.searchByContentFTS(keyword);
    results.reserve(matches.size());

    for (auto& [resourceId, snippet] : matches) {
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
    ResourceService::searchByContentUnifiedFull(std::string const& keyword) {
    auto results = m_resRepo.searchByContentUnified(keyword);

    for (auto& item : results) {
        item.displaySubText = Utils::normalizeSnippet(*item.rawSnippet);

        validateIsFile(item);
    }

    return results;
}

std::vector<UnifiedSearchResult> ResourceService::searchUnifiedFull(std::string_view tagLikeKW,
                                                                    std::string_view ftsKW,
                                                                    std::string_view domainLikeKW) {
    auto results = m_resRepo.searchUnified(tagLikeKW, ftsKW, domainLikeKW);

    for (auto& item : results) {
        validateIsFile(item);

        if (hasAnyFlags(item.flags, ResourceFlags::MatchText | ResourceFlags::MatchFileText) &&
            item.rawSnippet.has_value()) {
            item.displaySubText = Utils::normalizeSnippet(*item.rawSnippet);
        } else if (hasFlag(item.flags, ResourceFlags::MatchTag)) {
            std::vector<std::string> tagNames;
            auto tags = m_tagRepo.getTagsByResourceId(item.res.id);
            tagNames.reserve(tags.size());
            for (auto const& [id, name] : tags) {
                tagNames.push_back(name);
            }
            item.tags = tagNames;
            item.displaySubText = Utils::joinTags(item.tags);
        } else if (hasFlag(item.flags, ResourceFlags::MatchTitle)) {
            item.displaySubText = item.res.updated_at;
        } else if (hasAnyFlags(item.flags,
                               ResourceFlags::MatchDomain | ResourceFlags::MatchUrlPath)) {
            assert(item.url.has_value()); // invariant: domain match => URL resource
            item.displaySubText = *item.url;
        }
    }

    return results;
}

std::vector<Resource> ResourceService::getResourcesByTags(std::vector<std::string> const& tags) {
    return m_tagRepo.getResourcesViaTags(tags);
}

std::optional<std::string> ResourceService::getUrlByResourceIdOnly(sqlite3_int64 resourceId) const {
    return m_urlService.getUrlByResourceIdOnly(resourceId);
}

void ResourceService::addTagToResource(sqlite3_int64 resourceId, std::string const& tag) {
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
                                        std::vector<std::string> const& tagNames) {
    m_tagRepo.linkResourceWithTags(resourceId, tagNames);
}

void ResourceService::removeTagFromResource(sqlite3_int64 resourceId, std::string const& tag) {
    auto tagIdOpt = m_tagRepo.getTagIdByName(tag);
    if (!tagIdOpt.has_value()) [[unlikely]] {
        return;
    }

    m_tagRepo.deleteTagFromResource({.resourceId = resourceId, .tagId = *tagIdOpt});
}

std::vector<std::pair<sqlite3_int64, std::string>> ResourceService::getAllTags() {
    return m_tagRepo.getAllTags();
}

std::vector<Resource> ResourceService::getResourcesByTag(std::string const& tag) {
    return m_tagRepo.getResourcesViaOneTag(tag);
}

std::vector<UnifiedSearchResult> ResourceService::getFullResourcesByTag(std::string const& tag) {
    std::vector<UnifiedSearchResult> results;

    auto resources = m_tagRepo.getResourcesViaOneTag(tag);
    results.reserve(resources.size());

    for (auto const& res : resources) {
        UnifiedSearchResult u{};
        u.res = res;

        u.rawSnippet = std::nullopt;
        u.flags = ResourceFlags::MatchTag;

        auto full = getFullResource(res.id);
        if (full.has_value()) {
            u.tags = full->tags;
            if (!u.tags.empty()) {
                u.displaySubText = Utils::joinTags(u.tags);
            }

            validateIsFile(u);
        }

        results.push_back(std::move(u));
    }

    return results;
}

bool ResourceService::isExistTitle(std::string_view title, ResourceType type) const {
    return m_resRepo.existsTitle(title, type);
}

FullResource ResourceService::buildFullFromResource(Resource const& res) {
    FullResource fr;

    fr.resource = res;
    fr.content = m_textRepo.getTextById(res.id);
    fr.filepath = m_fileRepo.getFilepathByResourceId(res.id);

    auto tagPairs = m_tagRepo.getTagsByResourceId(res.id); // vector<pair<id,name>>
    fr.tags.reserve(tagPairs.size());

    for (auto& p : tagPairs) {
        fr.tags.push_back(p.second);
    }

    return fr;
}

std::vector<FullResource> ResourceService::getAllFull() {
    std::vector<FullResource> out;

    if (auto resources = m_resRepo.getAll(); !resources.empty()) {
        out.reserve(resources.size());
        for (auto& r : resources) {
            out.push_back(buildFullFromResource(r));
        }
    }

    return out;
}

std::vector<UnifiedSearchResult> ResourceService::getAllUnified() {
    std::vector<UnifiedSearchResult> out;

    auto resources = m_resRepo.getAll();
    out.reserve(resources.size());

    for (auto const& r : resources) {
        UnifiedSearchResult u{};
        u.res = r;

        auto const resId = r.id;

        // SubText: ngày cập nhật
        if (!r.updated_at.empty()) {
            u.displaySubText = r.updated_at;
        } else {
            u.displaySubText = r.created_at;
        }

        // Tags
        auto tagPairs = m_tagRepo.getTagsByResourceId(resId);
        for (auto& p : tagPairs) {
            u.tags.push_back(p.second);
        }

        u.flags = ResourceFlags::None;

        validateIsFile(u);

        // Url
        if (auto rawUrlOpt = getUrlByResourceIdOnly(resId)) {
            u.url = *rawUrlOpt;
            u.displaySubText.append(" - Url: " + *rawUrlOpt);
        }

        out.push_back(std::move(u));
    }

    return out;
}

void ResourceService::updateText(sqlite3_int64 resourceId, std::string_view newText) {
    m_textRepo.updateText(resourceId, newText);
}

std::vector<UnifiedSearchResult> ResourceService::getAllResourcesByType(ResourceType type) {
    auto out = m_resRepo.getAllResourcesByType(type);

    for (auto& res : out) {
        res.displaySubText = res.res.updated_at + " Tags: " + Utils::joinTags(res.tags);
        res.flags = ResourceFlags::None;

        validateIsFile(res);
    }

    return out;
}

bool ResourceService::isExistFile(sqlite3_int64 resourceId) const {
    return m_fileRepo.exists(resourceId);
}

void ResourceService::validateIsFile(UnifiedSearchResult& item) {
    auto const resId = item.res.id;
    if (isExistFile(resId)) {
        item.flags |= ResourceFlags::IsFile;

        if (auto fileOpt = m_fileRepo.getFileById(resId)) {
            item.filePath = fileOpt->stored_path;
            if (fileOpt->is_managed) {
                item.flags |= ResourceFlags::IsManaged;
            } else {
                item.flags |= ResourceFlags::IsExternal;
            }
        }
    }
}

std::optional<std::string> ResourceService::getResourceUuid(sqlite3_int64 resourceId) const {
    return m_resRepo.getResourceUuid(resourceId);
}
