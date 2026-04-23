#pragma once

#include "file_service.hpp"
#include "model.hpp"
#include "resource_service.hpp"
#include "text_content_repository.hpp"
#include "url_service.hpp"

#include <filesystem>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class NotesAppCore {
    public:
        NotesAppCore(TextContentRepository& textRepo, FileService& fileService,
                     UrlService& urlService, ResourceService& resService)
            : m_textRepo(textRepo), m_fileService(fileService), m_urlService(urlService),
              m_resService(resService) {}

        ~NotesAppCore() = default;

        // ========= CRUD =========
        [[nodiscard]]
        sqlite3_int64 addTextNote(std::string const& title, std::string const& content,
                                  ResourceType type) const;
        [[nodiscard]]
        sqlite3_int64 addFileNote(std::filesystem::path const& filepath, std::string const& title,
                                  ResourceType type, bool isManaged,
                                  std::string const& contentToIndex) const;
        [[nodiscard]]
        std::optional<sqlite3_int64> addUrlNote(std::string_view title, ResourceType type,
                                                std::string_view rawUrl) const;
        [[nodiscard]] std::optional<FullResource> getFullResource(sqlite3_int64 resourceId,
                                                                  bool includeContent = true) const;
        [[nodiscard]] std::vector<FullResource> getAllFull() const;

        [[nodiscard]] std::vector<UnifiedSearchResult> getAllUnified() const;

        void updateText(sqlite3_int64 resourceId, std::string_view newText);

        void deleteResource(sqlite3_int64 resourceId);
        void deleteResources(std::vector<sqlite3_int64> const& resourceIds);

        // ========= Search =========
        [[nodiscard]]
        std::vector<UnifiedSearchResult> searchByTitle(std::string const& keyword) const;
        [[nodiscard]]
        std::vector<std::pair<sqlite3_int64, std::string>>
            searchByContent(std::string const& keyword) const;

        // Tạm thời không sử dụng
        // [[nodiscard]] std::vector<FullResource>
        //     searchByContentFull(const std::string &keyword) const;
        // // =========

        [[nodiscard]]
        std::vector<UnifiedSearchResult>
            searchByContentUnifiedFull(std::string const& keyword) const;

        // For mode all
        [[nodiscard]]
        std::vector<UnifiedSearchResult> searchUnifiedFull(std::string_view tagLikeKW,
                                                           std::string_view ftsKW,
                                                           std::string_view domainLikeKW) const;

        [[nodiscard]]
        std::vector<UnifiedSearchResult> searchByTitleFull(std::string const& keyword) const;
        [[nodiscard]]
        std::vector<UnifiedSearchResult> getFullResourcesByTag(std::string const& tag) const;
        [[nodiscard]]
        std::vector<Resource> getResourcesByTags(std::vector<std::string> const& tags) const;

        // ========= Tags =========
        void addTag(sqlite3_int64 resourceId, std::string const& tag);
        void addTags(sqlite3_int64 resourceId, std::vector<std::string> const& tags);
        void removeTag(sqlite3_int64 resourceId, std::string const& tag);
        [[nodiscard]] std::vector<std::pair<sqlite3_int64, std::string>> getAllTags() const;

        // ========= Utility =========
        [[nodiscard]] bool isExistTitle(std::string_view title, ResourceType type) const;
        [[nodiscard]] bool isFileIndexed(std::filesystem::path const& filepath) const;
        [[nodiscard]] bool isExistFile(sqlite3_int64 resourceId) const;
        static std::string computeFileHash(std::filesystem::path const& filePath);

        std::vector<UnifiedSearchResult> getAllResourcesByType(ResourceType type);

    private:
        TextContentRepository& m_textRepo;
        FileService& m_fileService;
        UrlService& m_urlService;
        ResourceService& m_resService;
};
