#pragma once

#include <string>
#include <vector>
#include <utility>
#include <sqlite3.h>
#include "model.hpp"

class SQLiteDB;
class ResourceRepository;
class FileRepository;
class TextContentRepository;
class TagRepository;
class FileService;

class ResourceService {
    public:
        ResourceService(SQLiteDB &db, ResourceRepository &resRepo, FileRepository &fileRepo,
                        TextContentRepository &textRepo, TagRepository &tagRepo,
                        FileService &fileService) noexcept
            : m_db(db), m_resRepo(resRepo), m_fileRepo(fileRepo), m_textRepo(textRepo),
              m_tagRepo(tagRepo), m_fileService(fileService) {}

        // ========== CRUD ==========
        sqlite3_int64 addTextResource(const std::string &title, const std::string &content,
                                      ResourceType type);
        sqlite3_int64 addFileResource(const std::string &filepath, const std::string &title,
                                      ResourceType type, bool isManaged);
        std::optional<FullResource> getFullResource(sqlite3_int64 resourceId);
        void deleteResource(sqlite3_int64 resourceId);

        // ========== Search ==========
        std::vector<Resource> searchByTitle(const std::string &keyword);
        std::vector<FullResource> searchByTitleFull(const std::string &keyword);
        std::vector<std::pair<sqlite3_int64, std::string>>
            searchByContent(const std::string &keyword);
        std::vector<FullResource> searchByContentFull(const std::string &keyword);
        std::vector<Resource> getResourcesByTags(const std::vector<std::string> &tags);

        // ========== Tags ==========
        void addTagToResource(sqlite3_int64 resourceId, const std::string &tag);
        void addTagsToResource(sqlite3_int64 resourceId, const std::vector<std::string> &tagNames);

        void removeTagFromResource(sqlite3_int64 resourceId, const std::string &tag);
        std::vector<std::pair<sqlite3_int64, std::string>> getAllTags();
        std::vector<Resource> getResourcesByTag(const std::string &tag);
        std::vector<FullResource> getFullResourcesByTag(const std::string &tag);

        // ========= Utility =========
        [[nodiscard]] bool isExistTitle(std::string_view title, ResourceType type) const;

    private:
        SQLiteDB &m_db;
        ResourceRepository &m_resRepo;
        FileRepository &m_fileRepo;
        TextContentRepository &m_textRepo;
        TagRepository &m_tagRepo;
        FileService &m_fileService;
};
