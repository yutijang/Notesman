#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <utility>
#include <sqlite3.h>
#include "text_content_repository.hpp"
#include "sqldb_raii.hpp"

void TextContentRepository::insertText(sqlite3_int64 resourceId, std::string_view text) {
    SQLiteStmt stmt(m_db.get(), "INSERT INTO text_content(resource_id, content) VALUES (?, ?)");

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    sqlite3_bind_text(stmt.get(), 2, text.data(), static_cast<int>(text.size()), SQLITE_TRANSIENT);

    const int resCheck = sqlite3_step(stmt.get());
    if (resCheck != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Insert content failed for resource ID: " +
                                 std::to_string(resourceId) + " Error: " + erroMSG);
    }
}

std::optional<std::string> TextContentRepository::getTextById(sqlite3_int64 resourceId) {
    SQLiteStmt stmt(m_db.get(), "SELECT content FROM text_content WHERE resource_id = ?;");

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    const int resCheck = sqlite3_step(stmt.get());
    if (resCheck == SQLITE_ROW) {
        if (sqlite3_column_type(stmt.get(), 0) != SQLITE_NULL) {
            const char* textPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
            int len = sqlite3_column_bytes(stmt.get(), 0);
            return std::string(textPtr, static_cast<std::string::size_type>(len));
        }
        // Giá trị trả về trong lệnh truy vấn có chỉ số bắt đầu là 0 và vì chỉ
        // truy vấn 1 cột content nên giá trị iCol trong sqlite3_column_text() là 0
    }

    return std::nullopt;
}

void TextContentRepository::updateText(sqlite3_int64 resourceId, std::string_view newText) {
    SQLiteStmt stmt(m_db.get(), "UPDATE text_content SET content = ? WHERE resource_id = ?;");

    sqlite3_bind_text(stmt.get(), 1, newText.data(), static_cast<int>(newText.size()),
                      SQLITE_TRANSIENT); // NOLINT

    sqlite3_bind_int64(stmt.get(), 2, resourceId);

    const int resCheck = sqlite3_step(stmt.get());
    if (resCheck != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Update failed: " + erroMSG);
    }

    if (sqlite3_changes(m_db.get()) == 0) {
        throw std::runtime_error("Update failed: no rows updated for resource ID: " +
                                 std::to_string(resourceId));
    }
}

bool TextContentRepository::exists(sqlite3_int64 resourceId) {
    SQLiteStmt stmt(m_db.get(), "SELECT 1 FROM text_content WHERE resource_id = ? LIMIT 1;");

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    const int resCheck = sqlite3_step(stmt.get());

    return resCheck == SQLITE_ROW;
}

std::vector<std::pair<sqlite3_int64, std::string>> TextContentRepository::getAllTexts() {
    SQLiteStmt stmt(m_db.get(), "SELECT resource_id, content FROM text_content;");

    std::vector<std::pair<sqlite3_int64, std::string>> results;

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        sqlite3_int64 rID = sqlite3_column_int64(stmt.get(), 0);
        std::string content = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));

        results.emplace_back(rID, std::move(content));
    }

    return results;
}

std::vector<std::pair<sqlite3_int64, std::string>>
    TextContentRepository::searchByContentFTS(std::string_view keyword) {
    SQLiteStmt stmt(m_db.get(),
                    "SELECT rowid, content FROM text_content_fts WHERE text_content_fts MATCH ?;");

    sqlite3_bind_text(stmt.get(), 1, keyword.data(), static_cast<int>(keyword.size()),
                      SQLITE_TRANSIENT);

    std::vector<std::pair<sqlite3_int64, std::string>> result;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        sqlite_int64 rID = sqlite3_column_int64(stmt.get(), 0);
        std::string content = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        result.emplace_back(rID, std::move(content));
    }

    return result;
}
