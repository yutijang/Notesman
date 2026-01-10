#include <stdexcept>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <sqlite3.h>

#include "resource_repository.hpp"
#include "model.hpp"
#include "sqldb_raii.hpp"
#include "sqlite_utils.hpp"

sqlite3_int64 ResourceRepository::insert(const Resource &res) {
    SQLiteStmt stmt(m_db.get(), "INSERT INTO resources (title, type, file_hash) VALUES (?, ?, ?);");

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, res.title.c_str(), -1, SQLITE_TRANSIENT),
                      m_db.get());

    const char* typeStr = resourceTypeToString(res.type);
    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 2, typeStr, -1, SQLITE_TRANSIENT), m_db.get());

    if (res.type == ResourceType::plainText || res.file_hash.empty()) {
        sqlite::checkBind(sqlite3_bind_null(stmt.get(), 3), m_db.get());
    } else {
        sqlite::checkBind(
            sqlite3_bind_text(stmt.get(), 3, res.file_hash.c_str(), -1, SQLITE_TRANSIENT),
            m_db.get());
    }

    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE, "Insert resource");

    return sqlite3_last_insert_rowid(m_db.get());
}

std::optional<Resource> ResourceRepository::getById(sqlite3_int64 resourceId) {
    static constexpr const char* sql = "SELECT id, title, type, file_hash, created_at, updated_at "
                                       "FROM resources "
                                       "WHERE id = ?;";
    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    if (stmt.step() == SQLITE_ROW) { return resourceFromStmt(stmt); }

    return std::nullopt;
}

std::vector<Resource> ResourceRepository::getAll() {
    static constexpr const char* sql = "SELECT id, title, type, file_hash, created_at, updated_at "
                                       "FROM resources;";
    SQLiteStmt stmt(m_db.get(), sql);

    std::vector<Resource> results;
    while (stmt.step() == SQLITE_ROW) { results.push_back(resourceFromStmt(stmt)); }

    return results;
}

std::vector<UnifiedSearchResult> ResourceRepository::searchByTitleFTS(std::string_view keyword) {
    static constexpr const char* sql =
        "SELECT r.id, r.title, r.type, r.file_hash, r.created_at, r.updated_at "
        "FROM resources r "
        "JOIN resources_fts "
        "ON r.id = resources_fts.rowid "
        "WHERE resources_fts.title MATCH ?;";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, keyword.data(),
                                        static_cast<int>(keyword.size()), SQLITE_TRANSIENT),
                      m_db.get());

    std::vector<UnifiedSearchResult> result;
    while (stmt.step() == SQLITE_ROW) {
        Resource res = resourceFromStmt(stmt);

        UnifiedSearchResult ures;
        ures.res = std::move(res);
        ures.displaySubText = ures.res.title;
        ures.rawSnippet = std::nullopt;
        ures.flags = ResourceFlags::matchTitle;

        result.push_back(std::move(ures));
    }

    return result;
}

std::optional<Resource> ResourceRepository::getByFileHash(std::string_view hash) {
    SQLiteStmt stmt(m_db.get(), "SELECT id, title, type, file_hash, created_at, updated_at FROM "
                                "resources WHERE file_hash = ?;");

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, hash.data(), static_cast<int>(hash.size()),
                                        SQLITE_TRANSIENT),
                      m_db.get());

    const int rc = stmt.step();

    if (rc == SQLITE_ROW) { return resourceFromStmt(stmt); }

    if (rc == SQLITE_DONE) { return std::nullopt; }

    sqlite::checkStep(rc, m_db.get(), SQLITE_ROW, "getByFileHash");

    return std::nullopt;
}

std::optional<std::pair<std::string, std::string>>
    ResourceRepository::getTimestamps(sqlite3_int64 resourceID) {
    SQLiteStmt stmt(m_db.get(), "SELECT created_at, updated_at FROM resources WHERE id = ?;");

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceID), m_db.get());

    const int rc = stmt.step();

    if (rc == SQLITE_ROW) {
        auto createdAt = stmt.getColumnText(0);
        auto updatedAt = stmt.getColumnText(1);

        return std::make_pair(std::move(createdAt), std::move(updatedAt));
    }

    if (rc == SQLITE_DONE) { return std::nullopt; }

    sqlite::checkStep(rc, m_db.get(), SQLITE_ROW, "getTimestamps");

    return std::nullopt;
}

void ResourceRepository::update(const Resource &res) {
    SQLiteStmt stmt(m_db.get(), "UPDATE resources SET title = ?, type = ?, updated_at = "
                                "CURRENT_TIMESTAMP WHERE id = ?;");

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, res.title.c_str(), -1, SQLITE_TRANSIENT),
                      m_db.get());
    sqlite::checkBind(
        sqlite3_bind_text(stmt.get(), 2, resourceTypeToString(res.type), -1, SQLITE_TRANSIENT),
        m_db.get());
    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 3, res.id), m_db.get());
    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE, "Update");
}

void ResourceRepository::remove(sqlite3_int64 resourceId) {
    SQLiteStmt stmt(m_db.get(), "DELETE FROM resources WHERE id = ?;");

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());
    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE, "remove");
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
            sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, id), m_db.get());

            if (stmt.step() != SQLITE_DONE) {
                success = false;
                break;
            }

            stmt.reset();
            stmt.clearBindings();
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

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, hash.data(), static_cast<int>(hash.size()),
                                        SQLITE_TRANSIENT),
                      m_db.get());
    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 2, resourceID), m_db.get());
    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE, "updateFileHash");
}

bool ResourceRepository::existsTitle(std::string_view title, ResourceType type) const {
    SQLiteStmt stmt(
        m_db.get(),
        "SELECT EXISTS (SELECT 1 FROM resources WHERE title = ? AND type = ? LIMIT 1);");

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, title.data(), static_cast<int>(title.size()),
                                        SQLITE_TRANSIENT),
                      m_db.get());
    sqlite::checkBind(
        sqlite3_bind_text(stmt.get(), 2, resourceTypeToString(type), -1, SQLITE_TRANSIENT),
        m_db.get());

    if (stmt.step() == SQLITE_ROW) { return sqlite3_column_int(stmt.get(), 0) != 0; }

    return false;
}

std::vector<UnifiedSearchResult> ResourceRepository::searchUnified(std::string_view likeKW,
                                                                   std::string_view ftsKW) {
    static constexpr const char* sql = R"(
        SELECT r.id, r.title, r.type, r.file_hash, r.created_at, r.updated_at,
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

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, likeKW.data(),
                                        static_cast<int>(likeKW.size()), SQLITE_TRANSIENT),
                      m_db.get());
    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 2, ftsKW.data(), static_cast<int>(ftsKW.size()),
                                        SQLITE_TRANSIENT),
                      m_db.get());
    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 3, ftsKW.data(), static_cast<int>(ftsKW.size()),
                                        SQLITE_TRANSIENT),
                      m_db.get());
    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 4, ftsKW.data(), static_cast<int>(ftsKW.size()),
                                        SQLITE_TRANSIENT),
                      m_db.get());

    std::vector<UnifiedSearchResult> results;
    int rc{};
    while ((rc = stmt.step()) == SQLITE_ROW) {
        UnifiedSearchResult item{};
        item.res = resourceFromStmt(stmt);
        item.flags = ResourceFlags::none;

        auto snippet = stmt.getColumnText(7); // NOLINT(readability-magic-numbers)
        if (!snippet.empty()) {
            // Có snippet => match content
            item.displaySubText = snippet;
            item.rawSnippet = snippet;

            item.flags = item.flags | ResourceFlags::matchContent | ResourceFlags::hasSnippet;

            if (item.res.type != ResourceType::plainText) {
                item.flags = item.flags | ResourceFlags::isFile;
            }
        } else {
            // Không có snippet → Title hoặc Tag
            item.displaySubText = ""; // Delegate sẽ tự quyết
            item.flags = item.flags | ResourceFlags::matchTitle;
        }

        results.push_back(std::move(item));
    }

    sqlite::checkStep(rc, m_db.get(), SQLITE_DONE, "FTS Search");

    return results;
}

std::vector<UnifiedSearchResult>
    ResourceRepository::searchByContentUnified(std::string_view keyword) {
    static constexpr const char* sql = R"(
        SELECT r.id, r.title, r.type, r.file_hash, r.created_at, r.updated_at, 
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
        LIMIT 100;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, keyword.data(),
                                        static_cast<int>(keyword.size()), SQLITE_TRANSIENT),
                      m_db.get());
    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 2, keyword.data(),
                                        static_cast<int>(keyword.size()), SQLITE_TRANSIENT),
                      m_db.get());

    std::vector<UnifiedSearchResult> results;
    int rc{};
    while ((rc = stmt.step()) == SQLITE_ROW) {
        UnifiedSearchResult item{};
        item.res = (resourceFromStmt(stmt));

        // Bổ sung giá trị snippet
        auto snippet = stmt.getColumnText(7); // NOLINT(readability-magic-numbers)
        if (!snippet.empty()) {
            item.displaySubText = snippet;
            item.rawSnippet = snippet;
            item.flags = ResourceFlags::matchContent | ResourceFlags::hasSnippet;
        } else {
            item.flags = ResourceFlags::matchContent;
        }

        results.push_back(std::move(item));
    }

    sqlite::checkStep(rc, m_db.get(), SQLITE_DONE, "FTS Search");

    return results;
}

Resource ResourceRepository::resourceFromStmt(SQLiteStmt &stmt) {
    Resource res;

    res.id = stmt.getColumnInt64(0);
    res.title = stmt.getColumnText(1);

    auto type = stmt.getColumnText(2);
    res.type = type.empty() ? ResourceType::unknown : resourceTypeFromString(type);

    res.file_hash = stmt.getColumnText(3);
    res.created_at = stmt.getColumnText(4);
    res.updated_at = stmt.getColumnText(5); // NOLINT(readability-magic-numbers)

    return res;
}
