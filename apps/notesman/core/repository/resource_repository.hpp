#pragma once

#include "model.hpp"
#include "sqldb_raii.hpp"

#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class SQLiteDB;

class ResourceRepository {
  public:
    explicit ResourceRepository(SQLiteDB& db) noexcept : m_db(db) {}

    // CRUD
    sqlite3_int64 insert(Resource const& res);
    std::optional<Resource> getById(sqlite3_int64 resourceId);
    void update(Resource const& res);
    // Ràng buộc ON DELETE CASCADE
    // SQLite tự động xóa tất cả các hàng liên quan trong files, text_content, và resource_tags
    void remove(sqlite3_int64 resourceId);
    void removeBatch(std::vector<sqlite3_int64> const& resourceIds);

    std::vector<Resource> getAll();

    std::vector<UnifiedSearchResult> searchByTitleFTS(std::string_view keyword);

    std::vector<UnifiedSearchResult> searchByContentUnified(std::string_view keyword);
    std::vector<UnifiedSearchResult> searchUnified(std::string_view tagLikeKW,
                                                   std::string_view ftsKW,
                                                   std::string_view domainLikeKW);

    std::optional<Resource> getByFileHash(std::string_view hash);
    std::optional<std::pair<std::string, std::string>> getTimestamps(sqlite3_int64 resourceID);

    void updateFileHash(sqlite3_int64 resourceID, std::string_view hash);
    [[nodiscard]] bool existsTitle(std::string_view title, ResourceType type) const;

    std::vector<UnifiedSearchResult> getAllResourcesByType(ResourceType type);

    [[nodiscard]]
    std::optional<std::string> getResourceUuid(sqlite3_int64 resourceId) const;

  private:
    static Resource resourceFromStmt(SQLiteStmt const& stmt);
    static std::vector<std::string> splitTags(std::string_view s, std::string_view delimiter);

    SQLiteDB& m_db;
};
