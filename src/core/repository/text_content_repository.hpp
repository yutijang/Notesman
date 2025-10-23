#pragma once

#include <string_view>
#include <optional>
#include <vector>
#include <utility>
#include <sqlite3.h>

class SQLiteDB;

class TextContentRepository {
    public:
        explicit TextContentRepository(SQLiteDB &db) noexcept : m_db(db) {}

        void insertText(sqlite3_int64 resourceId, std::string_view text);
        std::vector<std::pair<sqlite3_int64, std::string>>
            searchByContentFTS(std::string_view keyword);
        std::optional<std::string> getTextById(sqlite3_int64 resourceId);
        std::vector<std::pair<sqlite3_int64, std::string>> getAllTexts();
        void updateText(sqlite3_int64 resourceId, std::string_view newText);

        bool exists(sqlite3_int64 resourceId);

    private:
        SQLiteDB &m_db;
};
