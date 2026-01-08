#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <sqlite3.h>

#include "model.hpp"
#include "sqldb_raii.hpp"

class SQLiteDB;

class ResourceRepository {
    public:
        explicit ResourceRepository(SQLiteDB &db) noexcept : m_db(db) {}

        // CRUD
        sqlite3_int64 insert(const Resource &res);
        std::optional<Resource> getById(sqlite3_int64 resourceId);
        void update(const Resource &res);
        // Ràng buộc ON DELETE CASCADE
        // SQLite tự động xóa tất cả các hàng liên quan trong files, text_content, và resource_tags
        void remove(sqlite3_int64 resourceId);
        void removeBatch(const std::vector<sqlite3_int64> &resourceIds);

        std::vector<Resource> getAll();

        // Tạm thời chưa sử dụng
        std::vector<Resource> searchByTitleFTS(std::string_view keyword);
        // =========

        std::vector<UnifiedSearchResult> searchByContentUnified(std::string_view keyword);
        std::vector<UnifiedSearchResult> searchUnified(std::string_view likeKW,
                                                       std::string_view ftsKW);

        std::optional<Resource> getByFileHash(std::string_view hash);
        std::optional<std::pair<std::string, std::string>> getTimestamps(sqlite3_int64 resourceID);

        void updateFileHash(sqlite3_int64 resourceID, std::string_view hash);
        [[nodiscard]] bool existsTitle(std::string_view title, ResourceType type) const;

    private:
        static Resource resourceFromStmt(sqlite3_stmt* stmt);

        SQLiteDB &m_db;
};
