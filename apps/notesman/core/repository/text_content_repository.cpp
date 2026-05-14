#include "core/repository/text_content_repository.hpp"

#include "common/logger/Logger.hpp"
#include "core/db/sqldb_raii.hpp"
#include "core/db/sqlite_utils.hpp"

#include <optional>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

void TextContentRepository::insertText(sqlite3_int64 resourceId, std::string_view text) {
    static constexpr char const* sql =
        "INSERT INTO text_content(resource_id, content) VALUES (?, ?)";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());
    sqlite::checkBind(
        sqlite3_bind_text(
            stmt.get(), 2, text.data(), static_cast<int>(text.size()), SQLITE_TRANSIENT),
        m_db.get());
    sqlite::checkStep(
        stmt.step(), m_db.get(), SQLITE_DONE, "Insert content failed for resource ID");
}

std::optional<std::string> TextContentRepository::getTextById(sqlite3_int64 resourceId) {
    static constexpr char const* sql = "SELECT content FROM text_content WHERE resource_id = ?;";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    int const rc = stmt.step();
    if (rc == SQLITE_ROW) {
        if (sqlite3_column_type(stmt.get(), 0) != SQLITE_NULL) {
            return stmt.getColumnText(0);
        }
        // Giá trị trả về trong lệnh truy vấn có chỉ số bắt đầu là 0 và vì chỉ
        // truy vấn 1 cột content nên giá trị iCol trong sqlite3_column_text() là 0
    }

    if (rc == SQLITE_DONE) {
        return std::nullopt;
    }

    sqlite::checkStep(rc, m_db.get(), SQLITE_ROW, "getTextById");

    return std::nullopt;
}

void TextContentRepository::updateText(sqlite3_int64 resourceId, std::string_view newText) {
    static constexpr char const* sql = "UPDATE text_content SET content = ? WHERE resource_id = ?;";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(
        sqlite3_bind_text(
            stmt.get(), 1, newText.data(), static_cast<int>(newText.size()), SQLITE_TRANSIENT),
        m_db.get());
    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 2, resourceId), m_db.get());
    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE, "updateText");

    if (sqlite3_changes(m_db.get()) == 0) {
        Log::err("Update failed: no rows updated for resource ID: {}", std::to_string(resourceId));
        throw std::runtime_error("Update failed: no rows updated for resource ID: " +
                                 std::to_string(resourceId));
    }
}

bool TextContentRepository::exists(sqlite3_int64 resourceId) {
    static constexpr char const* sql = "SELECT 1 FROM text_content WHERE resource_id = ? LIMIT 1;";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    return stmt.step() == SQLITE_ROW;
}

std::vector<std::pair<sqlite3_int64, std::string>> TextContentRepository::getAllTexts() {
    static constexpr char const* sql = "SELECT resource_id, content FROM text_content;";

    SQLiteStmt stmt(m_db.get(), sql);

    std::vector<std::pair<sqlite3_int64, std::string>> results;

    while (stmt.step() == SQLITE_ROW) {
        results.emplace_back(rowToEntry(stmt));
    }

    return results;
}

// Tạm thời không sử dụng
std::vector<std::pair<sqlite3_int64, std::string>>
    TextContentRepository::searchByContentFTS(std::string_view keyword) {
    static constexpr char const* sql = R"(
        SELECT
            tc.resource_id,
            tc.content
        FROM text_content_fts AS fts
        JOIN text_content AS tc
        ON tc.resource_id = fts.rowid
        WHERE text_content_fts MATCH ?
        ORDER BY bm25(text_content_fts);
    )";

    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(
        sqlite3_bind_text(
            stmt.get(), 1, keyword.data(), static_cast<int>(keyword.size()), SQLITE_TRANSIENT),
        m_db.get());

    std::vector<std::pair<sqlite3_int64, std::string>> result;
    while (stmt.step() == SQLITE_ROW) {
        result.emplace_back(rowToEntry(stmt));
    }

    return result;
}

std::pair<sqlite3_int64, std::string> TextContentRepository::rowToEntry(SQLiteStmt& stmt) {
    sqlite3_int64 rID = stmt.getColumnInt64(0);
    auto content = stmt.getColumnText(1);

    return {rID, std::move(content)};
}
