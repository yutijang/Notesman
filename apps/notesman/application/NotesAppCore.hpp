#pragma once

#include <filesystem>
#include <string>
#include <optional>
#include <string_view>
#include <vector>
#include <utility>
#include <sqlite3.h>

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
        [[nodiscard]] sqlite3_int64 addTextNote(const std::string &title,
                                                const std::string &content,
                                                ResourceType type) const;
        [[nodiscard]] sqlite3_int64 addFileNote(const std::filesystem::path &filepath,
                                                const std::string &title, ResourceType type,
                                                bool isManaged,
                                                const std::string &contentToIndex) const;
        [[nodiscard]] std::optional<FullResource> getFullResource(sqlite3_int64 resourceId) const;
        [[nodiscard]] std::vector<FullResource> getAllFull() const;

        [[nodiscard]] std::vector<UnifiedSearchResult> getAllUnified() const;

        void updateText(sqlite3_int64 resourceId, std::string_view newText);

        void deleteResource(sqlite3_int64 resourceId);
        void deleteResources(const std::vector<sqlite3_int64> &resourceIds);

        // ========= Search =========
        [[nodiscard]] std::vector<UnifiedSearchResult>
            searchByTitle(const std::string &keyword) const;
        [[nodiscard]] std::vector<std::pair<sqlite3_int64, std::string>>
            searchByContent(const std::string &keyword) const;

        // Tạm thời không sử dụng
        // [[nodiscard]] std::vector<FullResource>
        //     searchByContentFull(const std::string &keyword) const;
        // // =========

        [[nodiscard]] std::vector<UnifiedSearchResult>
            searchByContentUnifiedFull(const std::string &keyword) const;

        // For mode all
        [[nodiscard]] std::vector<UnifiedSearchResult>
            searchUnifiedFull(std::string_view likeKW, std::string_view ftsKW) const;

        [[nodiscard]] std::vector<UnifiedSearchResult>
            searchByTitleFull(const std::string &keyword) const;
        [[nodiscard]] std::vector<UnifiedSearchResult>
            getFullResourcesByTag(const std::string &tag) const;
        [[nodiscard]] std::vector<Resource>
            getResourcesByTags(const std::vector<std::string> &tags) const;

        // ========= Tags =========
        void addTag(sqlite3_int64 resourceId, const std::string &tag);
        void addTags(sqlite3_int64 resourceId, const std::vector<std::string> &tags);
        void removeTag(sqlite3_int64 resourceId, const std::string &tag);
        [[nodiscard]] std::vector<std::pair<sqlite3_int64, std::string>> getAllTags() const;

        // ========= Utility =========
        [[nodiscard]] bool isExistTitle(std::string_view title, ResourceType type) const;
        [[nodiscard]] bool isFileIndexed(const std::filesystem::path &filepath) const;
        static std::string computeFileHash(const std::filesystem::path &filePath);

        std::vector<UnifiedSearchResult> getAllResourcesByType(ResourceType type);

    private:
        SQLiteDB &m_db;
        ResourceRepository &m_resRepo;
        FileRepository &m_fileRepo;
        TextContentRepository &m_textRepo;
        TagRepository &m_tagRepo;
        FileService &m_fileService;
        ResourceService &m_resService;
};
