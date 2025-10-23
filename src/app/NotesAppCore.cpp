#include "NotesAppCore.hpp"
#include "resource_service.hpp"
#include "file_service.hpp"

// ========= CRUD =========
sqlite3_int64 NotesAppCore::addTextNote(const std::string &title, const std::string &content,
                                        ResourceType type) {
    return m_resService.addTextResource(title, content, type);
}

sqlite3_int64 NotesAppCore::addFileNote(const std::string &filepath, const std::string &title,
                                        ResourceType type, bool isManaged) {
    return m_fileService.addFileResource(filepath, title, type, isManaged);
}

std::optional<FullResource> NotesAppCore::getFullResource(sqlite3_int64 resourceId) {
    return m_resService.getFullResource(resourceId);
}

void NotesAppCore::deleteResource(sqlite3_int64 resourceId) {
    m_resService.deleteResource(resourceId);
}

// ========= Search =========
std::vector<Resource> NotesAppCore::searchByTitle(const std::string &keyword) {
    return m_resService.searchByTitle(keyword);
}

std::vector<std::pair<sqlite3_int64, std::string>>
    NotesAppCore::searchByContent(const std::string &keyword) {
    return m_resService.searchByContent(keyword);
}

std::vector<FullResource> NotesAppCore::searchByContentFull(const std::string &keyword) {
    return m_resService.searchByContentFull(keyword);
}

std::vector<FullResource> NotesAppCore::searchByTitleFull(const std::string &keyword) {
    return m_resService.searchByTitleFull(keyword);
}

std::vector<FullResource> NotesAppCore::getFullResourcesByTag(const std::string &tag) {
    return m_resService.getFullResourcesByTag(tag);
}

std::vector<Resource> NotesAppCore::getResourcesByTags(const std::vector<std::string> &tags) {
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

std::vector<std::pair<sqlite3_int64, std::string>> NotesAppCore::getAllTags() {
    return m_resService.getAllTags();
}

bool NotesAppCore::isExistTitle(std::string_view title, ResourceType type) const {
    return m_resService.isExistTitle(title, type);
}

bool NotesAppCore::isFileIndexed(const std::string &filepath) const {
    auto resId = m_fileService.findResourceByFile(filepath);
    return resId.has_value();
}
