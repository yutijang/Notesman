#include <string_view>
#include <sqlite3.h>

#include "file_text_content_repository.hpp"
#include "sqldb_raii.hpp"

void FileTextContentRepository::upsertText(sqlite3_int64 resourceId, std::string_view text) {
    static constexpr const char* sql =
        "INSERT INTO file_text_content (resource_id, content, extracted_at) "
        "VALUES (?, ?, CURRENT_TIMESTAMP) "
        "ON CONFLICT(resource_id) DO UPDATE SET "
        "content = excluded.content, "
        "extracted_at = CURRENT_TIMESTAMP "
        "WHERE content IS NOT excluded.content;";
    SQLiteStmt stmt(m_db.get(), sql);

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    sqlite3_bind_text64(stmt.get(), 2, text.data(), static_cast<sqlite3_uint64>(text.size()),
                        SQLITE_TRANSIENT, SQLITE_UTF8);

    const int resCheck = sqlite3_step(stmt.get());
    if (resCheck != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Upsert content failed for resource ID: " +
                                 std::to_string(resourceId) + " Error: " + erroMSG);
    }
}

std::optional<std::string> FileTextContentRepository::getTextById(sqlite3_int64 resourceId) {
    static constexpr const char* sql =
        "SELECT content FROM file_text_content WHERE resource_id = ?;";
    SQLiteStmt stmt(m_db.get(), sql);

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    const int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
        const auto* textPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        if (textPtr != nullptr) {
            int bytes = sqlite3_column_bytes(stmt.get(), 0);
            return std::string(textPtr, static_cast<std::size_t>(bytes));
        }
    }

    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Error fetching file text content: " +
                                 std::string(sqlite3_errmsg(m_db.get())));
    }

    return std::nullopt;
}

bool FileTextContentRepository::isIndexed(sqlite3_int64 resourceId) {
    static constexpr const char* sql =
        "SELECT 1 FROM file_text_content WHERE resource_id = ? LIMIT 1;";
    SQLiteStmt stmt(m_db.get(), sql);

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    const int rc = sqlite3_step(stmt.get());

    return rc == SQLITE_ROW;
}
