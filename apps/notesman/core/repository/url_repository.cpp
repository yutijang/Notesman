#include <optional>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "url_repository.hpp"
#include "model.hpp"
#include "sqldb_raii.hpp"
#include "sqlite_utils.hpp"

void UrlRepository::insertUrl(sqlite3_int64 resourceId, std::string_view url,
                              std::string_view normalizedUrl, std::string_view domain,
                              std::string_view urlPath) {
    static constexpr const char* sql = R"(
        INSERT INTO resource_urls (
            resource_id,
            url,
            normalized_url,
            domain,
            url_path
        )
        VALUES (?, ?, ?, ?, ?);
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 2, url.data(), static_cast<int>(url.size()),
                                        SQLITE_TRANSIENT),
                      m_db.get());

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 3, normalizedUrl.data(),
                                        static_cast<int>(normalizedUrl.size()), SQLITE_TRANSIENT),
                      m_db.get());

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 4, domain.data(),
                                        static_cast<int>(domain.size()), SQLITE_TRANSIENT),
                      m_db.get());

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 5, urlPath.data(),
                                        static_cast<int>(urlPath.size()), SQLITE_TRANSIENT),
                      m_db.get());

    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE,
                      "insertUrl, resource id: " + std::to_string(resourceId));
}

void UrlRepository::updateUrl(sqlite3_int64 resourceId, std::string_view url,
                              std::string_view normalizedUrl, std::string_view domain,
                              std::string_view urlPath) {
    static constexpr const char* sql = R"(
        UPDATE resource_urls
        SET
            url = ?,
            normalized_url = ?,
            domain = ?,
            url_path = ?
        WHERE resource_id = ?;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, url.data(), static_cast<int>(url.size()),
                                        SQLITE_TRANSIENT),
                      m_db.get());

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 2, normalizedUrl.data(),
                                        static_cast<int>(normalizedUrl.size()), SQLITE_TRANSIENT),
                      m_db.get());

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 3, domain.data(),
                                        static_cast<int>(domain.size()), SQLITE_TRANSIENT),
                      m_db.get());

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 4, urlPath.data(),
                                        static_cast<int>(urlPath.size()), SQLITE_TRANSIENT),
                      m_db.get());

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 5, resourceId), m_db.get());

    const int rc = stmt.step();
    if (rc == SQLITE_CONSTRAINT) {
        throw std::runtime_error("updateUrl: unique constraint violated, url=" + std::string(url));
    }
    sqlite::checkStep(rc, m_db.get(), SQLITE_DONE, "updateUrl");

    if (sqlite3_changes(m_db.get()) == 0) {
        throw std::runtime_error("Update failed: no rows updated for resource ID: " +
                                 std::to_string(resourceId));
    }
}

std::optional<UrlEntry> UrlRepository::getUrlByResourceId(sqlite3_int64 resourceId) const {
    static constexpr const char* sql = R"(
        SELECT
            resource_id,
            url,
            normalized_url,
            domain
        FROM resource_urls
        WHERE resource_id = ?;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    const int rc = stmt.step();
    if (rc == SQLITE_ROW) {
        UrlEntry entry{};

        entry.resource_id = stmt.getColumnInt64(0);
        entry.url = stmt.getColumnText(1);
        entry.normalized_url = stmt.getColumnText(2);
        entry.domain = stmt.getColumnText(3);

        return entry;
    }

    sqlite::checkStep(rc, m_db.get(), SQLITE_DONE, "getUrlByResourceId");

    return std::nullopt;
}

std::vector<UrlEntry> UrlRepository::getAllUrls() const {
    static constexpr const char* sql = R"(
        SELECT
            resource_id,
            url,
            normalized_url,
            domain
        FROM resource_urls;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    std::vector<UrlEntry> results;

    while (stmt.step() == SQLITE_ROW) { results.emplace_back(urlEntryFromStmt(stmt)); }

    return results;
}

std::optional<sqlite3_int64>
    UrlRepository::getResourceIdByNormalizedUrl(std::string_view normalizedUrl) const {
    static constexpr const char* sql = R"(
        SELECT
            resource_id
        FROM resource_urls
        WHERE normalized_url = ?
        LIMIT 1;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, normalizedUrl.data(),
                                        static_cast<int>(normalizedUrl.size()), SQLITE_TRANSIENT),
                      m_db.get());

    if (stmt.step() == SQLITE_ROW) { return stmt.getColumnInt64(0); }

    return std::nullopt;
}

std::vector<sqlite3_int64> UrlRepository::getResourceIdsByDomain(std::string_view domain) const {
    static constexpr const char* sql = R"(
        SELECT
            resource_id
        FROM resource_urls
        WHERE domain = ?;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, domain.data(),
                                        static_cast<int>(domain.size()), SQLITE_TRANSIENT),
                      m_db.get());

    std::vector<sqlite3_int64> results;
    while (stmt.step() == SQLITE_ROW) { results.emplace_back(stmt.getColumnInt64(0)); }

    return results;
}

std::optional<std::string> UrlRepository::getUrlByResourceIdOnly(sqlite3_int64 resourceId) const {
    static constexpr const char* sql = R"(
        SELECT
            url
        FROM resource_urls
        WHERE resource_id = ?
        LIMIT 1;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    if (stmt.step() == SQLITE_ROW) { return stmt.getColumnText(0); }

    return std::nullopt;
}

std::optional<std::string> UrlRepository::getDomainByResourceId(sqlite3_int64 resourceId) const {
    static constexpr const char* sql = R"(
        SELECT
            domain
        FROM resource_urls
        WHERE resource_id = ?
        LIMIT 1;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    if (stmt.step() == SQLITE_ROW) { return stmt.getColumnText(0); }

    return std::nullopt;
}

bool UrlRepository::exists(sqlite3_int64 resourceId) const {
    static constexpr const char* sql = R"(
        SELECT 1
        FROM resource_urls
        WHERE resource_id = ?
        LIMIT 1;
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    return stmt.step() == SQLITE_ROW;
}

// === helper ===
UrlEntry UrlRepository::urlEntryFromStmt(const SQLiteStmt &stmt) {
    UrlEntry entry{};

    entry.resource_id = stmt.getColumnInt64(0);
    entry.url = stmt.getColumnText(1);
    entry.normalized_url = stmt.getColumnText(2);
    entry.domain = stmt.getColumnText(3);

    return entry;
}

// =========