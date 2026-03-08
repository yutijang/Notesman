#include "file_text_content_repository.hpp"

#include "sqldb_raii.hpp"
#include "sqlite_utils.hpp"

#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>

void FileTextContentRepository::upsertText(sqlite3_int64 resourceId, std::string_view text) {
    static constexpr char const* sql =
        "INSERT INTO file_text_content (resource_id, content, extracted_at) "
        "VALUES (?, ?, CURRENT_TIMESTAMP) "
        "ON CONFLICT(resource_id) DO UPDATE SET "
        "content = excluded.content, "
        "extracted_at = CURRENT_TIMESTAMP "
        "WHERE content IS NOT excluded.content;";
    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    sqlite::checkBind(sqlite3_bind_text64(stmt.get(), 2, text.data(),
                                          static_cast<sqlite3_uint64>(text.size()),
                                          SQLITE_TRANSIENT, SQLITE_UTF8),
                      m_db.get());

    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE,
                      "Upsert - Resource ID: " + std::to_string(resourceId));
}

std::optional<std::string> FileTextContentRepository::getTextById(sqlite3_int64 resourceId) {
    static constexpr char const* sql =
        "SELECT content FROM file_text_content WHERE resource_id = ?;";
    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    int const rc = stmt.step();
    if (rc == SQLITE_ROW) { return stmt.getColumnText(0); }

    sqlite::checkStep(rc, m_db.get(), SQLITE_DONE, "getTextById");

    return std::nullopt;
}

bool FileTextContentRepository::isIndexed(sqlite3_int64 resourceId) {
    static constexpr char const* sql =
        "SELECT 1 FROM file_text_content WHERE resource_id = ? LIMIT 1;";
    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    return stmt.step() == SQLITE_ROW;
}
