#include <stdexcept>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <sqlite3.h>

#include "resource_repository.hpp"
#include "Logger.hpp"
#include "model.hpp"
#include "sqldb_raii.hpp"

sqlite3_int64 ResourceRepository::insert(const Resource &res) {
    SQLiteStmt stmt(m_db.get(), "INSERT INTO resources (title, type, file_hash) VALUES (?, ?, ?);");

    sqlite3_bind_text(stmt.get(), 1, res.title.c_str(), -1, SQLITE_TRANSIENT);

    const char* typeStr = resourceTypeToString(res.type);
    sqlite3_bind_text(stmt.get(), 2, typeStr, -1, SQLITE_TRANSIENT);

    if (res.type == ResourceType::plainText || res.file_hash.empty()) {
        sqlite3_bind_null(stmt.get(), 3);
    } else {
        sqlite3_bind_text(stmt.get(), 3, res.file_hash.c_str(), -1, SQLITE_TRANSIENT);
    }

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Insert failed for resource: " + res.title + " Error: " + erroMSG);
    }

    return sqlite3_last_insert_rowid(m_db.get());
}

std::optional<Resource> ResourceRepository::getById(sqlite3_int64 resourceId) {
    static constexpr const char* sql = "SELECT id, title, type, created_at, updated_at "
                                       "FROM resources "
                                       "WHERE id = ?;";
    SQLiteStmt stmt(m_db.get(), sql);

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) { return resourceFromStmt(stmt.get()); }

    return std::nullopt;
}

std::vector<Resource> ResourceRepository::getAll() {
    static constexpr const char* sql = "SELECT id, title, type, created_at, updated_at "
                                       "FROM resources;";
    SQLiteStmt stmt(m_db.get(), sql);

    std::vector<Resource> results;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        results.push_back(resourceFromStmt(stmt.get()));
    }

    return results;
}

std::vector<Resource> ResourceRepository::searchByTitleFTS(std::string_view keyword) {
    static constexpr const char* sql =
        "SELECT r.id, r.title, r.type, r.file_hash, r.created_at, r.updated_at "
        "FROM resources r "
        "JOIN resources_fts "
        "ON r.id = resources_fts.rowid "
        "WHERE resources_fts.title MATCH ?;";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite3_bind_text(stmt.get(), 1, keyword.data(), static_cast<int>(keyword.size()),
                      SQLITE_TRANSIENT);

    std::vector<Resource> result;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        Resource res;
        res.id = sqlite3_column_int64(stmt.get(), 0);
        res.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));

        const char* typeText = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        res.type = resourceTypeFromString(typeText);

        if (sqlite3_column_type(stmt.get(), 3) != SQLITE_NULL) {
            res.file_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
        }

        res.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));
        res.updated_at = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt.get(), 5)); // NOLINT(readability-magic-numbers)

        result.push_back(std::move(res));
    }

    return result;
}

std::optional<Resource> ResourceRepository::getByFileHash(std::string_view hash) {
    SQLiteStmt stmt(m_db.get(), "SELECT id, title, type, file_hash, created_at, updated_at FROM "
                                "resources WHERE file_hash = ?;");

    sqlite3_bind_text(stmt.get(), 1, hash.data(), static_cast<int>(hash.size()), SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        Resource res;
        res.id = sqlite3_column_int64(stmt.get(), 0);
        res.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));

        const char* typeText = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        res.type = resourceTypeFromString(typeText);

        if (sqlite3_column_type(stmt.get(), 3) != SQLITE_NULL) {
            res.file_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
        }

        res.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));
        res.updated_at = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt.get(), 5)); // NOLINT(readability-magic-numbers)

        return res;
    }

    return std::nullopt;
}

std::optional<std::pair<std::string, std::string>>
    ResourceRepository::getTimestamps(sqlite3_int64 resourceID) {
    SQLiteStmt stmt(m_db.get(), "SELECT created_at, updated_at FROM resources WHERE id = ?;");

    sqlite3_bind_int64(stmt.get(), 1, resourceID);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        std::string createdAt;
        std::string updatedAt;

        if (sqlite3_column_type(stmt.get(), 0) != SQLITE_NULL) {
            createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        }

        if (sqlite3_column_type(stmt.get(), 1) != SQLITE_NULL) {
            updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        }

        return std::make_pair(std::move(createdAt), std::move(updatedAt));
    }

    return std::nullopt;
}

void ResourceRepository::update(const Resource &res) {
    SQLiteStmt stmt(m_db.get(), "UPDATE resources SET title = ?, type = ?, updated_at = "
                                "CURRENT_TIMESTAMP WHERE id = ?;");

    sqlite3_bind_text(stmt.get(), 1, res.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, resourceTypeToString(res.type), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 3, res.id);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Update failed: " + erroMSG);
    }
}

void ResourceRepository::remove(sqlite3_int64 resourceId) {
    SQLiteStmt stmt(m_db.get(), "DELETE FROM resources WHERE id = ?;");

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Delete failed: " + erroMSG);
    }
}

void ResourceRepository::removeBatch(const std::vector<sqlite3_int64> &resourceIds) {
    if (resourceIds.empty()) { return; }

    char* errMsg{};
    if (sqlite3_exec(m_db.get(), "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg = (errMsg != nullptr) ? errMsg : "Unknown transaction begin error";
        sqlite3_free(errMsg);
        throw std::runtime_error("Failed to begin transaction: " + msg);
    }

    SQLiteStmt stmt(m_db.get(), "DELETE FROM resources WHERE id = ?;");

    bool success = true;
    {
        for (const auto id : resourceIds) {
            sqlite3_bind_int64(stmt.get(), 1, id);

            if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
                success = false;
                break;
            }

            sqlite3_reset(stmt.get());
            sqlite3_clear_bindings(stmt.get());
        }
    }

    const char* endSql = success ? "COMMIT;" : "ROLLBACK;";
    if (sqlite3_exec(m_db.get(), endSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg = (errMsg != nullptr) ? errMsg : "Unknown transaction end error";
        sqlite3_free(errMsg);
        throw std::runtime_error("Failed to finalize transaction: " + msg);
    }

    if (!success) { throw std::runtime_error("Failed to delete some resources"); }
}

void ResourceRepository::updateFileHash(sqlite3_int64 resourceID, std::string_view hash) {
    SQLiteStmt stmt(m_db.get(), "UPDATE resources SET file_hash = ? WHERE id = ?;");

    sqlite3_bind_text(stmt.get(), 1, hash.data(), static_cast<int>(hash.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 2, resourceID);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Update failed: " + erroMSG);
    }
}

bool ResourceRepository::existsTitle(std::string_view title, ResourceType type) const {
    SQLiteStmt stmt(
        m_db.get(),
        "SELECT EXISTS (SELECT 1 FROM resources WHERE title = ? AND type = ? LIMIT 1);");

    sqlite3_bind_text(stmt.get(), 1, title.data(), static_cast<int>(title.size()),
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, resourceTypeToString(type), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) { return sqlite3_column_int(stmt.get(), 0) != 0; }

    return false;
}

std::vector<UnifiedSearchResult> ResourceRepository::searchUnified(std::string_view likeKW,
                                                                   std::string_view ftsKW) {
    static constexpr const char* sql = R"(
        SELECT r.id, r.title, r.type, r.created_at, r.updated_at,
               MIN(m.score) AS final_score,
               m.snip AS snippet
        FROM resources r
        JOIN (
            -- Khối 1: Tìm theo Tag (Không có snippet văn bản)            
            SELECT rt.resource_id AS row_id, -10.0 AS score, '' AS snip
            FROM tags t
            JOIN resource_tags rt ON rt.tag_id = t.id WHERE t.name LIKE ?

            UNION ALL

            -- Khối 2: Tìm theo Title (Không có snippet văn bản)
            SELECT rowid AS row_id, bm25(resources_fts) AS score, '' AS snip
            FROM resources_fts WHERE resources_fts MATCH ?

            UNION ALL

            -- Khối 3: Tìm trong nội dung tài nguyên text thuần (Có snippet)
            SELECT rowid AS row_id, bm25(text_content_fts) + 5.0 AS score,
                   snippet(text_content_fts, 0, '[', ']', '...', 20) AS snip
            FROM text_content_fts WHERE text_content_fts MATCH ?

            UNION ALL

            -- Khối 4: Tìm trong nội dung File (Có snippet)
            SELECT rowid AS row_id, bm25(file_text_content_fts) + 10.0 AS score,
                   snippet(file_text_content_fts, 0, '[', ']', '...', 20) AS snip
            FROM file_text_content_fts WHERE file_text_content_fts MATCH ?
        ) m ON r.id = m.row_id
        GROUP BY r.id
        ORDER BY final_score ASC, r.updated_at DESC
        LIMIT 100;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite3_bind_text(stmt.get(), 1, likeKW.data(), static_cast<int>(likeKW.size()),
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, ftsKW.data(), static_cast<int>(ftsKW.size()),
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 3, ftsKW.data(), static_cast<int>(ftsKW.size()),
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 4, ftsKW.data(), static_cast<int>(ftsKW.size()),
                      SQLITE_TRANSIENT);

    std::vector<UnifiedSearchResult> results;
    int rc{};
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        UnifiedSearchResult item;
        item.res = resourceFromStmt(stmt.get());

        const char* snipPtr = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt.get(), 6)); // NOLINT(readability-magic-numbers)
        item.snippet = (snipPtr != nullptr) ? snipPtr : "";

        results.push_back(std::move(item));
    }

    return results;
}

Resource ResourceRepository::resourceFromStmt(sqlite3_stmt* stmt) {
    Resource res;

    res.id = sqlite3_column_int64(stmt, 0);

    const auto* titlePtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    res.title = (titlePtr != nullptr) ? titlePtr : "";

    const auto* typePtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    if (typePtr != nullptr) {
        res.type = resourceTypeFromString(typePtr);
    } else {
        res.type = ResourceType::unknown;
    }

    const auto* createdPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    res.created_at = (createdPtr != nullptr) ? createdPtr : "";

    const auto* updatedPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    res.updated_at = (updatedPtr != nullptr) ? updatedPtr : "";

    return res;
}

std::vector<UnifiedSearchResult>
    ResourceRepository::searchByContentUnified(std::string_view keyword) {
    static constexpr const char* sql = R"(
        SELECT r.id, r.title, r.type, r.created_at, r.updated_at, 
            MIN(m.score) AS final_score, 
            m.snip AS snippet
        FROM resources r
        JOIN (
            -- Khối 1: Tìm trong ghi chú tay
            SELECT rowid AS row_id,
                   bm25(text_content_fts) AS score,
                   snippet(text_content_fts, 0, '[', ']', '...', 20) AS snip
            FROM text_content_fts WHERE text_content_fts MATCH ?
            
            UNION ALL
            
            -- Khối 2: Tìm trong nội dung file (code, txt...)
            SELECT rowid AS row_id, 
                   bm25(file_text_content_fts) + 1.0 AS score,
                   snippet(file_text_content_fts, 0, '[', ']', '...', 20) AS snip
            FROM file_text_content_fts WHERE file_text_content_fts MATCH ?
            
            -- Tương lai bổ sung Khối 3: PDF/EPUB tại đây
        ) m ON r.id = m.row_id
        GROUP BY r.id
        ORDER BY final_score ASC, r.updated_at DESC
        LIMIT 100
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite3_bind_text(stmt.get(), 1, keyword.data(), static_cast<int>(keyword.size()),
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, keyword.data(), static_cast<int>(keyword.size()),
                      SQLITE_TRANSIENT);

    std::vector<UnifiedSearchResult> results;
    int rc{};
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        UnifiedSearchResult item;
        item.res = (resourceFromStmt(stmt.get()));

        // Bổ sung giá trị snippet
        const char* snipPtr = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt.get(), 6)); // NOLINT(readability-magic-numbers)
        if (snipPtr != nullptr) { item.snippet = snipPtr; }

        results.push_back(std::move(item));
    }

    if (rc != SQLITE_DONE) { Log::err("FTS Search Error: {}", sqlite3_errmsg(m_db.get())); }

    return results;
}
