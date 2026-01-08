#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <utility>
#include <string_view>
#include <sqlite3.h>

#include "NotesAppCore.hpp"
#include "resource_service.hpp"
#include "file_service.hpp"
#include "model.hpp"

// ========= CRUD =========
sqlite3_int64 NotesAppCore::addTextNote(const std::string &title, const std::string &content,
                                        ResourceType type) const {
    return m_resService.addTextResource(title, content, type);
}

sqlite3_int64 NotesAppCore::addFileNote(const std::filesystem::path &filepath,
                                        const std::string &title, ResourceType type, bool isManaged,
                                        const std::string &contentToIndex) const {
    return m_fileService.addFileResource(filepath, title, type, isManaged, contentToIndex);
}

std::optional<FullResource> NotesAppCore::getFullResource(sqlite3_int64 resourceId) const {
    return m_resService.getFullResource(resourceId);
}

std::vector<FullResource> NotesAppCore::getAllFull() const {
    return m_resService.getAllFull();
}

void NotesAppCore::updateText(sqlite3_int64 resourceId, std::string_view newText) {
    m_textRepo.updateText(resourceId, newText);
}

void NotesAppCore::deleteResource(sqlite3_int64 resourceId) {
    m_resService.deleteResource(resourceId);
}

void NotesAppCore::deleteResources(const std::vector<sqlite3_int64> &resourceIds) {
    m_resService.deleteResources(resourceIds);
}

// ========= Search =========
std::vector<Resource> NotesAppCore::searchByTitle(const std::string &keyword) const {
    return m_resService.searchByTitle(keyword);
}

std::vector<std::pair<sqlite3_int64, std::string>>
    NotesAppCore::searchByContent(const std::string &keyword) const {
    return m_resService.searchByContent(keyword);
}

// std::vector<FullResource> NotesAppCore::searchByContentFull(const std::string &keyword) const {
//     return m_resService.searchByContentFull(keyword);
// }

std::vector<FullResource>
    NotesAppCore::searchByContentUnifiedFull(const std::string &keyword) const {
    return m_resService.searchByContentUnifiedFull(keyword);
}

std::vector<FullResource> NotesAppCore::searchUnifiedFull(std::string_view likeKW,
                                                          std::string_view ftsKW) const {
    return m_resService.searchUnifiedFull(likeKW, ftsKW);
}

std::vector<FullResource> NotesAppCore::searchByTitleFull(const std::string &keyword) const {
    return m_resService.searchByTitleFull(keyword);
}

std::vector<FullResource> NotesAppCore::getFullResourcesByTag(const std::string &tag) const {
    return m_resService.getFullResourcesByTag(tag);
}

std::vector<Resource> NotesAppCore::getResourcesByTags(const std::vector<std::string> &tags) const {
    return m_resService.getResourcesByTags(tags);
}

// ========= Tags =========
void NotesAppCore::addTag(sqlite3_int64 resourceId, const std::string &tag) {
    m_resService.addTagToResource(resourceId, tag);
}

void NotesAppCore::addTags(sqlite3_int64 resourceId, const std::vector<std::string> &tags) {
    m_resService.addTagsToResource(resourceId, tags);
}

void NotesAppCore::removeTag(sqlite3_int64 resourceId, const std::string &tag) {
    m_resService.removeTagFromResource(resourceId, tag);
}

std::vector<std::pair<sqlite3_int64, std::string>> NotesAppCore::getAllTags() const {
    return m_resService.getAllTags();
}

bool NotesAppCore::isExistTitle(std::string_view title, ResourceType type) const {
    return m_resService.isExistTitle(title, type);
}

bool NotesAppCore::isFileIndexed(const std::filesystem::path &filepath) const {
    auto resId = m_fileService.findResourceByFile(filepath);
    return resId.has_value();
}

std::string NotesAppCore::computeFileHash(const std::filesystem::path &filePath) {
    return FileService::computeFileHash(filePath);
}
