#pragma once

#include <string>
#include <optional>
#include <vector>
#include <utility>
#include "model.hpp"
#include "file_repository.hpp"
#include "file_service.hpp"
#include "resource_repository.hpp"
#include "resource_service.hpp"
#include "sqldb_raii.hpp"
#include "tag_repository.hpp"
#include "text_content_repository.hpp"

class NotesAppCore {
    public:
        NotesAppCore(SQLiteDB &db, ResourceRepository &resRepo, FileRepository &fileRepo,
                     TextContentRepository &textRepo, TagRepository &tagRepo,
                     FileService &fileService, ResourceService &resService)
            : m_db(db), m_resRepo(resRepo), m_fileRepo(fileRepo), m_textRepo(textRepo),
              m_tagRepo(tagRepo), m_fileService(fileService), m_resService(resService) {}

        ~NotesAppCore() = default;

        // ========= CRUD =========
        sqlite3_int64 addTextNote(const std::string &title, const std::string &content,
                                  ResourceType type);
        sqlite3_int64 addFileNote(const std::string &filepath, const std::string &title,
                                  ResourceType type, bool isManaged);
        std::optional<FullResource> getFullResource(sqlite3_int64 resourceId);
        void deleteResource(sqlite3_int64 resourceId);

        // ========= Search =========
        std::vector<Resource> searchByTitle(const std::string &keyword);
        std::vector<std::pair<sqlite3_int64, std::string>>
            searchByContent(const std::string &keyword);
        std::vector<FullResource> searchByContentFull(const std::string &keyword);
        std::vector<FullResource> searchByTitleFull(const std::string &keyword);
        std::vector<FullResource> getFullResourcesByTag(const std::string &tag);
        std::vector<Resource> getResourcesByTags(const std::vector<std::string> &tags);

        // ========= Tags =========
        void addTag(sqlite3_int64 resourceId, const std::string &tag);
        void addTags(sqlite3_int64 resourceId, const std::vector<std::string> &tags);
        void removeTag(sqlite3_int64 resourceId, const std::string &tag);
        std::vector<std::pair<sqlite3_int64, std::string>> getAllTags();

        // ========= Utility =========
        [[nodiscard]] bool isExistTitle(std::string_view title, ResourceType type) const;
        [[nodiscard]] bool isFileIndexed(const std::string &filepath) const;

    private:
        SQLiteDB &m_db;
        ResourceRepository &m_resRepo;
        FileRepository &m_fileRepo;
        TextContentRepository &m_textRepo;
        TagRepository &m_tagRepo;
        FileService &m_fileService;
        ResourceService &m_resService;
};
