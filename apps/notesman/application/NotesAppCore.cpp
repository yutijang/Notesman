#include "NotesAppCore.hpp"

#include "file_service.hpp"
#include "model.hpp"
#include "resource_service.hpp"

#include <filesystem>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// ========= CRUD =========
sqlite3_int64 NotesAppCore::addTextNote(std::string const& title, std::string const& content,
                                        ResourceType type) const {
    return m_resService.addTextResource(title, content, type);
}

sqlite3_int64 NotesAppCore::addFileNote(std::filesystem::path const& filepath,
                                        std::string const& title, ResourceType type, bool isManaged,
                                        std::string const& contentToIndex) const {
    return m_fileService.addFileResource(filepath, title, type, isManaged, contentToIndex);
}

std::optional<sqlite3_int64> NotesAppCore::addUrlNote(std::string_view title, ResourceType type,
                                                      std::string_view rawUrl) const {
    return m_resService.addUrlResource(title, type, rawUrl);
}

std::optional<FullResource> NotesAppCore::getFullResource(sqlite3_int64 resourceId,
                                                          bool includeContent) const {
    return m_resService.getFullResource(resourceId, includeContent);
}

std::vector<FullResource> NotesAppCore::getAllFull() const {
    return m_resService.getAllFull();
}

std::vector<UnifiedSearchResult> NotesAppCore::getAllUnified() const {
    return m_resService.getAllUnified();
}

void NotesAppCore::updateText(sqlite3_int64 resourceId, std::string_view newText) {
    m_textRepo.updateText(resourceId, newText);
}

void NotesAppCore::deleteResource(sqlite3_int64 resourceId) {
    m_resService.deleteResource(resourceId);
}

void NotesAppCore::deleteResources(std::vector<sqlite3_int64> const& resourceIds) {
    m_resService.deleteResources(resourceIds);
}

std::optional<std::string> NotesAppCore::getResourceUuid(sqlite3_int64 resourceId) const {
    return m_resService.getResourceUuid(resourceId);
}

// ========= Search =========
std::vector<UnifiedSearchResult> NotesAppCore::searchByTitle(std::string const& keyword) const {
    return m_resService.searchByTitle(keyword);
}

std::vector<std::pair<sqlite3_int64, std::string>>
    NotesAppCore::searchByContent(std::string const& keyword) const {
    return m_resService.searchByContent(keyword);
}

// std::vector<FullResource> NotesAppCore::searchByContentFull(const std::string &keyword) const {
//     return m_resService.searchByContentFull(keyword);
// }

std::vector<UnifiedSearchResult>
    NotesAppCore::searchByContentUnifiedFull(std::string const& keyword) const {
    return m_resService.searchByContentUnifiedFull(keyword);
}

std::vector<UnifiedSearchResult>
    NotesAppCore::searchUnifiedFull(std::string_view tagLikeKW, std::string_view ftsKW,
                                    std::string_view domainLikeKW) const {
    return m_resService.searchUnifiedFull(tagLikeKW, ftsKW, domainLikeKW);
}

std::vector<UnifiedSearchResult> NotesAppCore::searchByTitleFull(std::string const& keyword) const {
    return m_resService.searchByTitleFull(keyword);
}

std::vector<UnifiedSearchResult> NotesAppCore::getFullResourcesByTag(std::string const& tag) const {
    return m_resService.getFullResourcesByTag(tag);
}

std::vector<Resource> NotesAppCore::getResourcesByTags(std::vector<std::string> const& tags) const {
    return m_resService.getResourcesByTags(tags);
}

// ========= Tags =========
void NotesAppCore::addTag(sqlite3_int64 resourceId, std::string const& tag) {
    m_resService.addTagToResource(resourceId, tag);
}

void NotesAppCore::addTags(sqlite3_int64 resourceId, std::vector<std::string> const& tags) {
    m_resService.addTagsToResource(resourceId, tags);
}

void NotesAppCore::removeTag(sqlite3_int64 resourceId, std::string const& tag) {
    m_resService.removeTagFromResource(resourceId, tag);
}

std::vector<std::pair<sqlite3_int64, std::string>> NotesAppCore::getAllTags() const {
    return m_resService.getAllTags();
}

bool NotesAppCore::isExistTitle(std::string_view title, ResourceType type) const {
    return m_resService.isExistTitle(title, type);
}

bool NotesAppCore::isFileIndexed(std::filesystem::path const& filepath) const {
    auto resId = m_fileService.findResourceByFile(filepath);
    return resId.has_value();
}

std::string NotesAppCore::computeFileHash(std::filesystem::path const& filePath) {
    return FileService::computeFileHash(filePath);
}

std::vector<UnifiedSearchResult> NotesAppCore::getAllResourcesByType(ResourceType type) {
    return m_resService.getAllResourcesByType(type);
}

bool NotesAppCore::isExistFile(sqlite3_int64 resourceId) const {
    return m_resService.isExistFile(resourceId);
}
