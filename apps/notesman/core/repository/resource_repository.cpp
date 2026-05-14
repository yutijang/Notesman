#include "core/repository/resource_repository.hpp"

#include "common/logger/Logger.hpp"
#include "core/db/sqldb_raii.hpp"
#include "core/db/sqlite_utils.hpp"
#include "core/model/model.hpp"

#include <optional>
#include <ranges>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

sqlite3_int64 ResourceRepository::insert(Resource const& res) {
    static constexpr char const* sql = R"(
        INSERT INTO resources (title, type, file_hash)
        VALUES (?, ?, ?);
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, res.title.c_str(), -1, SQLITE_TRANSIENT),
                      m_db.get());

    auto const typeStr = resourceTypeToString(res.type);
    sqlite::checkBind(
        sqlite3_bind_text(
            stmt.get(), 2, typeStr.data(), static_cast<int>(typeStr.size()), SQLITE_TRANSIENT),
        m_db.get());

    if (res.type == ResourceType::PlainText || res.file_hash.empty()) {
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
    static constexpr char const* sql = R"(
        SELECT
            id,
            uuid,
            title,
            type,
            file_hash,
            created_at,
            updated_at
        FROM resources
        WHERE id = ?;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    if (stmt.step() == SQLITE_ROW) {
        return resourceFromStmt(stmt);
    }

    return std::nullopt;
}

std::optional<std::string> ResourceRepository::getResourceUuid(sqlite3_int64 resourceId) const {
    static constexpr char const* sql = "SELECT uuid FROM resources WHERE id = ?;";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    if (stmt.step() == SQLITE_ROW) {
        return stmt.getColumnText(0);
    }

    return std::nullopt;
}

std::vector<Resource> ResourceRepository::getAll() {
    static constexpr char const* sql = R"(
        SELECT
            id,
            uuid,
            title,
            type,
            file_hash,
            created_at,
            updated_at
        FROM resources;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    std::vector<Resource> results;
    while (stmt.step() == SQLITE_ROW) {
        results.push_back(resourceFromStmt(stmt));
    }

    return results;
}

std::vector<UnifiedSearchResult> ResourceRepository::searchByTitleFTS(std::string_view keyword) {
    static constexpr char const* sql = R"(
        SELECT
            r.id,
            r.uuid,
            r.title,
            r.type,
            r.file_hash,
            r.created_at,
            r.updated_at
        FROM resources r
        JOIN resources_fts ON r.id = resources_fts.rowid
        WHERE resources_fts.title MATCH ?;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(
        sqlite3_bind_text(
            stmt.get(), 1, keyword.data(), static_cast<int>(keyword.size()), SQLITE_TRANSIENT),
        m_db.get());

    std::vector<UnifiedSearchResult> result;
    while (stmt.step() == SQLITE_ROW) {
        Resource res = resourceFromStmt(stmt);

        std::string updatedAt = res.updated_at;

        UnifiedSearchResult ures{.res = std::move(res),
                                 .displaySubText = updatedAt,
                                 .rawSnippet = std::nullopt,
                                 .filePath = std::nullopt,
                                 .url = std::nullopt,
                                 .tags = {},
                                 .flags = ResourceFlags::MatchTitle};

        result.push_back(std::move(ures));
    }

    return result;
}

std::optional<Resource> ResourceRepository::getByFileHash(std::string_view hash) {
    static constexpr char const* sql = R"(
        SELECT
            id,
            uuid,
            title,
            type,
            file_hash,
            created_at,
            updated_at
        FROM resources
        WHERE file_hash = ?;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(
        sqlite3_bind_text(
            stmt.get(), 1, hash.data(), static_cast<int>(hash.size()), SQLITE_TRANSIENT),
        m_db.get());

    int const rc = stmt.step();

    if (rc == SQLITE_ROW) {
        return resourceFromStmt(stmt);
    }

    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }

    sqlite::checkStep(rc, m_db.get(), SQLITE_ROW, "getByFileHash");

    return std::nullopt;
}

std::optional<std::pair<std::string, std::string>>
    ResourceRepository::getTimestamps(sqlite3_int64 resourceID) {
    static constexpr char const* sql = R"(
        SELECT
            created_at,
            updated_at
        FROM resources
        WHERE id = ?;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceID), m_db.get());

    int const rc = stmt.step();

    if (rc == SQLITE_ROW) {
        auto createdAt = stmt.getColumnText(0);
        auto updatedAt = stmt.getColumnText(1);

        return std::make_pair(std::move(createdAt), std::move(updatedAt));
    }

    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }

    sqlite::checkStep(rc, m_db.get(), SQLITE_ROW, "getTimestamps");

    return std::nullopt;
}

void ResourceRepository::update(Resource const& res) {
    static constexpr char const* sql = R"(
        UPDATE resources
        SET title = ?,
            type = ?,
            updated_at = CURRENT_TIMESTAMP
        WHERE id = ?;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, res.title.c_str(), -1, SQLITE_TRANSIENT),
                      m_db.get());

    auto const typeStr = resourceTypeToString(res.type);
    sqlite::checkBind(
        sqlite3_bind_text(
            stmt.get(), 2, typeStr.data(), static_cast<int>(typeStr.size()), SQLITE_TRANSIENT),
        m_db.get());
    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 3, res.id), m_db.get());
    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE, "Update");
}

void ResourceRepository::remove(sqlite3_int64 resourceId) {
    static constexpr char const* sql = "DELETE FROM resources WHERE id = ?;";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());
    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE, "remove");
}

void ResourceRepository::removeBatch(std::vector<sqlite3_int64> const& resourceIds) {
    if (resourceIds.empty()) {
        Log::err("resourceIds must not be empty");
        throw std::runtime_error("resourceIds must not be empty");
    }

    char* errMsg{};
    if (sqlite3_exec(m_db.get(), "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg = (errMsg != nullptr) ? errMsg : "Unknown transaction begin error";
        sqlite3_free(errMsg);
        throw std::runtime_error("Failed to begin transaction: " + msg);
    }

    SQLiteStmt stmt(m_db.get(), "DELETE FROM resources WHERE id = ?;");

    bool success = true;
    {
        for (auto const id : resourceIds) {
            sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, id), m_db.get());

            if (stmt.step() != SQLITE_DONE) {
                success = false;
                break;
            }

            stmt.reset();
            stmt.clearBindings();
        }
    }

    char const* endSql = success ? "COMMIT;" : "ROLLBACK;";
    if (sqlite3_exec(m_db.get(), endSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string msg = (errMsg != nullptr) ? errMsg : "Unknown transaction end error";
        sqlite3_free(errMsg);
        throw std::runtime_error("Failed to finalize transaction: " + msg);
    }

    if (!success) {
        throw std::runtime_error("Failed to delete some resources");
    }
}

void ResourceRepository::updateFileHash(sqlite3_int64 resourceID, std::string_view hash) {
    static constexpr char const* sql = "UPDATE resources SET file_hash = ? WHERE id = ?;";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(
        sqlite3_bind_text(
            stmt.get(), 1, hash.data(), static_cast<int>(hash.size()), SQLITE_TRANSIENT),
        m_db.get());
    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 2, resourceID), m_db.get());
    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE, "updateFileHash");
}

bool ResourceRepository::existsTitle(std::string_view title, ResourceType type) const {
    static constexpr char const* sql = R"(
        SELECT EXISTS (
            SELECT 1
            FROM resources
            WHERE title = ?
            AND type = ?
            LIMIT 1
        );
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(
        sqlite3_bind_text(
            stmt.get(), 1, title.data(), static_cast<int>(title.size()), SQLITE_TRANSIENT),
        m_db.get());

    auto const typeStr = resourceTypeToString(type);
    sqlite::checkBind(
        sqlite3_bind_text(
            stmt.get(), 2, typeStr.data(), static_cast<int>(typeStr.size()), SQLITE_TRANSIENT),
        m_db.get());

    if (stmt.step() == SQLITE_ROW) {
        return sqlite3_column_int(stmt.get(), 0) != 0;
    }

    return false;
}

/*
Quy ước bitmask:

| Bit | Ý nghĩa          |
| --- | ---------------- |
| 1   | matchTag         |
| 2   | matchTitle       |
| 4   | matchContentText |
| 8   | matchContentFile |

*/
std::vector<UnifiedSearchResult> ResourceRepository::searchUnified(std::string_view tagLikeKW,
                                                                   std::string_view ftsKW,
                                                                   std::string_view domainLikeKW) {
    sqlite::checkExec(m_db.get(), "BEGIN", "begin search tx");
    try {
        // ---------------------------------------------------------------------
        // Phase 0: create temp table
        // ---------------------------------------------------------------------
        {
            static constexpr char const* sqlCreateTmp = R"(
                CREATE TEMP TABLE IF NOT EXISTS tmp_search_ids (
                    resource_id INTEGER PRIMARY KEY,
                    score REAL NOT NULL,
                    match_reason INTEGER NOT NULL
                );
            )";

            sqlite::checkExec(m_db.get(), sqlCreateTmp, "create tmp_search_ids");
        }

        // ---------------------------------------------------------------------
        // Phase 0 next: clear temp table
        // ---------------------------------------------------------------------
        {
            static constexpr char const* sqlClearTmp = R"(
                DELETE FROM tmp_search_ids;
            )";

            sqlite::checkExec(m_db.get(), sqlClearTmp, "clear tmp_search_ids");
        }

        // ---------------------------------------------------------------------
        // Phase 1: insert matched resource ids
        // ---------------------------------------------------------------------
        {
            static constexpr char const* sqlPhase1 = R"(
                INSERT INTO tmp_search_ids(resource_id, score, match_reason)
                SELECT row_id, MIN(score), SUM(match_reason)
                FROM (
                    -- Tag
                    SELECT rt.resource_id AS row_id, -10.0 AS score, 1 AS match_reason
                    FROM tags t
                    JOIN resource_tags rt ON rt.tag_id = t.id
                    WHERE t.name LIKE ?

                    UNION ALL
                    -- Title
                    SELECT rowid, bm25(resources_fts), 2
                    FROM resources_fts
                    WHERE resources_fts MATCH ?

                    UNION ALL
                    -- Text content
                    SELECT rowid, bm25(text_content_fts) + 5.0, 4
                    FROM text_content_fts
                    WHERE text_content_fts MATCH ?

                    UNION ALL
                    -- File text
                    SELECT rowid, bm25(file_text_content_fts) + 10.0, 8
                    FROM file_text_content_fts
                    WHERE file_text_content_fts MATCH ?

                    UNION ALL
                    -- URL path
                    SELECT rowid, bm25(resource_url_path_fts) + 7.0, 32
                    FROM resource_url_path_fts
                    WHERE resource_url_path_fts MATCH ?

                    UNION ALL
                    -- Domain
                    SELECT ru.resource_id, -5.0, 16
                    FROM resource_urls ru
                    WHERE ru.url LIKE ?
                )
                GROUP BY row_id
                ORDER BY MIN(score)
                LIMIT 100;
            )";

            SQLiteStmt stmt(m_db.get(), sqlPhase1);

            sqlite::checkBind(sqlite3_bind_text(stmt.get(),
                                                1,
                                                tagLikeKW.data(),
                                                static_cast<int>(tagLikeKW.size()),
                                                SQLITE_TRANSIENT),
                              m_db.get());

            for (int i = 2; i <= 5; ++i) {
                sqlite::checkBind(sqlite3_bind_text(stmt.get(),
                                                    i,
                                                    ftsKW.data(),
                                                    static_cast<int>(ftsKW.size()),
                                                    SQLITE_TRANSIENT),
                                  m_db.get());
            }

            sqlite::checkBind(sqlite3_bind_text(stmt.get(),
                                                6,
                                                domainLikeKW.data(),
                                                static_cast<int>(domainLikeKW.size()),
                                                SQLITE_TRANSIENT),
                              m_db.get());

            int rc = stmt.step();
            sqlite::checkStep(rc, m_db.get(), SQLITE_DONE, "searchUnified phase1");
        }

        // ---------------------------------------------------------------------
        // Phase 2: hydrate resource data
        // ---------------------------------------------------------------------
        static constexpr char const* sqlPhase2 = R"(
            SELECT
                r.id,
                r.uuid,
                r.title,
                r.type,
                r.file_hash,
                r.created_at,
                r.updated_at,
                ru.url AS url,
                t.score AS final_score,

                CASE
                    WHEN (t.match_reason & 4) != 0 THEN (
                        SELECT snippet(text_content_fts, 0, '[', ']', '...', 30)
                        FROM text_content_fts
                        WHERE rowid = r.id
                        AND text_content_fts MATCH ?
                    )
                    WHEN (t.match_reason & 8) != 0 THEN (
                        SELECT snippet(file_text_content_fts, 0, '[', ']', '...', 30)
                        FROM file_text_content_fts
                        WHERE rowid = r.id
                        AND file_text_content_fts MATCH ?
                    )
                    ELSE ''
                END AS snippet,

                t.match_reason
            FROM tmp_search_ids t
            JOIN resources r ON r.id = t.resource_id
            LEFT JOIN resource_urls ru ON ru.resource_id = r.id
            ORDER BY t.score ASC, r.updated_at DESC;
        )";

        SQLiteStmt stmt(m_db.get(), sqlPhase2);

        sqlite::checkBind(
            sqlite3_bind_text(
                stmt.get(), 1, ftsKW.data(), static_cast<int>(ftsKW.size()), SQLITE_TRANSIENT),
            m_db.get());

        sqlite::checkBind(
            sqlite3_bind_text(
                stmt.get(), 2, ftsKW.data(), static_cast<int>(ftsKW.size()), SQLITE_TRANSIENT),
            m_db.get());

        std::vector<UnifiedSearchResult> results;
        results.reserve(100);
        int rc{};
        while ((rc = stmt.step()) == SQLITE_ROW) {
            UnifiedSearchResult item{};
            item.res = resourceFromStmt(stmt);

            int const reason = sqlite3_column_int(stmt.get(), 10);

            ResourceFlags flags{};
            if ((reason & 1) != 0) {
                flags |= ResourceFlags::MatchTag;
            }
            if ((reason & 2) != 0) {
                flags |= ResourceFlags::MatchTitle;
            }
            if ((reason & 4) != 0) {
                flags |= ResourceFlags::MatchText;
            }
            if ((reason & 8) != 0) {
                flags |= ResourceFlags::MatchFileText;
            }
            if ((reason & 16) != 0) {
                flags |= ResourceFlags::MatchDomain;
            }
            if ((reason & 32) != 0) {
                flags |= ResourceFlags::MatchUrlPath;
            }

            item.flags = flags;

            if (hasAnyFlags(item.flags, ResourceFlags::MatchText | ResourceFlags::MatchFileText)) {
                // NOLINTNEXTLINE(readability-magic-numbers)
                if (auto snippet = stmt.getColumnText(9); !snippet.empty()) {
                    item.rawSnippet = snippet;
                    item.flags |= ResourceFlags::HasSnippet;
                }
            }

            item.url = stmt.getColumnText(7);

            results.push_back(std::move(item));
        }

        sqlite::checkStep(rc, m_db.get(), SQLITE_DONE, "searchUnified phase2");

        sqlite::checkExec(m_db.get(), "COMMIT", "commit search tx");

        return results;
    } catch (...) {
        try {
            sqlite::checkExec(m_db.get(), "ROLLBACK", "rollback search tx");
        } catch (...) {
        }

        throw;
    }
}

std::vector<UnifiedSearchResult>
    ResourceRepository::searchByContentUnified(std::string_view keyword) {
    static constexpr char const* sql = R"(
        SELECT r.id, r.uuid, r.title, r.type, r.file_hash, r.created_at, r.updated_at, 
            MIN(m.score) AS final_score, 
            m.snip AS snippet
        FROM resources r
        JOIN (
            -- Khối 1: Tìm trong ghi chú tay
            SELECT rowid AS row_id,
                   bm25(text_content_fts) AS score,
                   snippet(text_content_fts, 0, '[', ']', '...', 30) AS snip
            FROM text_content_fts WHERE text_content_fts MATCH ?
            
            UNION ALL
            
            -- Khối 2: Tìm trong nội dung file (code, txt...)
            SELECT rowid AS row_id, 
                   bm25(file_text_content_fts) + 1.0 AS score,
                   snippet(file_text_content_fts, 0, '[', ']', '...', 30) AS snip
            FROM file_text_content_fts WHERE file_text_content_fts MATCH ?
            
            -- Tương lai bổ sung Khối 3: PDF/EPUB tại đây
        ) m ON r.id = m.row_id
        GROUP BY r.id
        ORDER BY final_score ASC, r.updated_at DESC
        LIMIT 100;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(
        sqlite3_bind_text(
            stmt.get(), 1, keyword.data(), static_cast<int>(keyword.size()), SQLITE_TRANSIENT),
        m_db.get());
    sqlite::checkBind(
        sqlite3_bind_text(
            stmt.get(), 2, keyword.data(), static_cast<int>(keyword.size()), SQLITE_TRANSIENT),
        m_db.get());

    std::vector<UnifiedSearchResult> results;
    results.reserve(100);
    int rc{};
    while ((rc = stmt.step()) == SQLITE_ROW) {
        Resource res = resourceFromStmt(stmt);

        auto snippet = stmt.getColumnText(8); // NOLINT(readability-magic-numbers)

        UnifiedSearchResult item{.res = std::move(res),
                                 .displaySubText = {},
                                 .rawSnippet = snippet,
                                 .filePath = std::nullopt,
                                 .url = std::nullopt,
                                 .tags = {},
                                 .flags = ResourceFlags::MatchText | ResourceFlags::MatchFileText |
                                          ResourceFlags::HasSnippet};

        results.push_back(std::move(item));
    }

    sqlite::checkStep(rc, m_db.get(), SQLITE_DONE, "searchByContentUnified");

    return results;
}

Resource ResourceRepository::resourceFromStmt(SQLiteStmt const& stmt) {
    Resource res{};

    res.id = stmt.getColumnInt64(0);
    res.uuid = stmt.getColumnText(1);
    res.title = stmt.getColumnText(2);

    auto type = stmt.getColumnText(3);
    res.type = type.empty() ? ResourceType::Unknown : resourceTypeFromString(type);

    res.file_hash = stmt.getColumnText(4);
    res.created_at = stmt.getColumnText(5); // NOLINT(readability-magic-numbers)
    res.updated_at = stmt.getColumnText(6); // NOLINT(readability-magic-numbers)

    return res;
}

std::vector<UnifiedSearchResult> ResourceRepository::getAllResourcesByType(ResourceType type) {
    static constexpr char const* sql = R"(
        SELECT
            r.id,
            r.uuid,
            r.title,
            r.updated_at,
            ru.url AS raw_url,
            GROUP_CONCAT(t.name, ', ') AS tag_list
        FROM resources AS r
        LEFT JOIN resource_urls AS ru
            ON ru.resource_id = r.id
        LEFT JOIN resource_tags AS rt
            ON rt.resource_id = r.id
        LEFT JOIN tags AS t
            ON t.id = rt.tag_id
        WHERE r.type = ?
        GROUP BY r.id
        ORDER BY r.updated_at DESC;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    auto const typeStr = resourceTypeToString(type);
    sqlite::checkBind(
        sqlite3_bind_text(
            stmt.get(), 1, typeStr.data(), static_cast<int>(typeStr.size()), SQLITE_TRANSIENT),
        m_db.get());

    std::vector<UnifiedSearchResult> results;
    int rc{};
    while ((rc = stmt.step()) == SQLITE_ROW) {
        UnifiedSearchResult item{};

        item.res.id = stmt.getColumnInt64(0);
        item.res.uuid = stmt.getColumnText(1);
        item.res.title = stmt.getColumnText(2);
        item.res.type = type;
        item.res.updated_at = stmt.getColumnText(3);

        if (std::string url = stmt.getColumnText(4); !url.empty()) {
            item.url = url;
        }

        if (std::string tagStr = stmt.getColumnText(5); !tagStr.empty()) {
            item.tags = splitTags(tagStr, ", ");
        }

        results.push_back(std::move(item));
    }

    sqlite::checkStep(rc, m_db.get(), SQLITE_DONE, "getAllResourcesByType");

    return results;
}

std::vector<std::string> ResourceRepository::splitTags(std::string_view s,
                                                       std::string_view delimiter) {
    auto view = s | std::ranges::views::split(std::string_view{delimiter}) |
                std::ranges::views::transform([](auto&& r) {
                    return std::string(r.begin(), r.end());
                });

    return {view.begin(), view.end()};
}
