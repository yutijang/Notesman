#pragma once

#include <optional>
#include <string>
#include <vector>
#include <utility>
#include <string_view>
#include <sqlite3.h>

#include "model.hpp"
#include "sqldb_raii.hpp"
#include "resource_repository.hpp"
#include "file_repository.hpp"
#include "text_content_repository.hpp"
#include "tag_repository.hpp"
#include "file_service.hpp"
#include "url_service.hpp"

class ResourceService {
    public:
        ResourceService(SQLiteDB &db, ResourceRepository &resRepo, FileRepository &fileRepo,
                        TextContentRepository &textRepo, TagRepository &tagRepo,
                        FileService &fileService, UrlService &urlService) noexcept
            : m_db(db), m_resRepo(resRepo), m_fileRepo(fileRepo), m_textRepo(textRepo),
              m_tagRepo(tagRepo), m_fileService(fileService), m_urlService(urlService) {}

        // ========== CRUD ==========
        sqlite3_int64 addTextResource(const std::string &title, const std::string &content,
                                      ResourceType type);
        sqlite3_int64 addFileResource(const std::string &filepath, const std::string &title,
                                      ResourceType type, bool isManaged,
                                      const std::string &contentToIndex);
        std::optional<sqlite3_int64> addUrlResource(std::string_view title, ResourceType type,
                                                    std::string_view rawUrl);
        std::optional<FullResource> getFullResource(sqlite3_int64 resourceId,
                                                    bool includeContent = true);
        std::vector<FullResource> getAllFull();

        std::vector<UnifiedSearchResult> getAllUnified();

        void updateText(sqlite3_int64 resourceId, std::string_view newText);

        void deleteResource(sqlite3_int64 resourceId);
        void deleteResources(const std::vector<sqlite3_int64> &resourceIds);

        // ========== Search ==========
        std::vector<UnifiedSearchResult> searchByTitle(const std::string &keyword);
        std::vector<UnifiedSearchResult> searchByTitleFull(const std::string &keyword);

        std::vector<std::pair<sqlite3_int64, std::string>>
            searchByContent(const std::string &keyword);

        // Tạm thời không sử dụng
        std::vector<FullResource> searchByContentFull(const std::string &keyword);
        // =========

        std::vector<UnifiedSearchResult> searchByContentUnifiedFull(const std::string &keyword);
        std::vector<UnifiedSearchResult> searchUnifiedFull(std::string_view tagLikeKW,
                                                           std::string_view ftsKW,
                                                           std::string_view domainLikeKW);

        std::vector<Resource> getResourcesByTags(const std::vector<std::string> &tags);

        [[nodiscard]]
        std::optional<std::string> getUrlByResourceIdOnly(sqlite3_int64 resourceId) const;

        // ========== Tags ==========
        void addTagToResource(sqlite3_int64 resourceId, const std::string &tag);
        void addTagsToResource(sqlite3_int64 resourceId, const std::vector<std::string> &tagNames);

        void removeTagFromResource(sqlite3_int64 resourceId, const std::string &tag);
        std::vector<std::pair<sqlite3_int64, std::string>> getAllTags();
        std::vector<Resource> getResourcesByTag(const std::string &tag);
        std::vector<UnifiedSearchResult> getFullResourcesByTag(const std::string &tag);

        std::vector<UnifiedSearchResult> getAllResourcesByType(ResourceType type);

        // ========= Utility =========
        [[nodiscard]] bool isExistTitle(std::string_view title, ResourceType type) const;
        [[nodiscard]] bool isExistFile(sqlite3_int64 resourceId) const;

    private:
        FullResource buildFullFromResource(const Resource &res);
        void validateIsFile(UnifiedSearchResult &item);

        SQLiteDB &m_db;
        ResourceRepository &m_resRepo;
        FileRepository &m_fileRepo;
        TextContentRepository &m_textRepo;
        TagRepository &m_tagRepo;
        FileService &m_fileService;
        UrlService &m_urlService;
};
