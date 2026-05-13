#pragma once

#include "sqldb_raii.hpp"

#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class TextContentRepository {
  public:
    explicit TextContentRepository(SQLiteDB& db) noexcept : m_db(db) {}

    void insertText(sqlite3_int64 resourceId, std::string_view text);

    // Tạm thời không sử dụng
    std::vector<std::pair<sqlite3_int64, std::string>> searchByContentFTS(std::string_view keyword);
    // =========

    std::optional<std::string> getTextById(sqlite3_int64 resourceId);
    std::vector<std::pair<sqlite3_int64, std::string>> getAllTexts();
    void updateText(sqlite3_int64 resourceId, std::string_view newText);

    bool exists(sqlite3_int64 resourceId);

  private:
    static std::pair<sqlite3_int64, std::string> rowToEntry(SQLiteStmt& stmt);

    SQLiteDB& m_db;
};
