#pragma once

#include <string>
#include <vector>
#include <sqlite3.h>

class SQLiteDB;

class DatabaseChecker {
    public:
        explicit DatabaseChecker(SQLiteDB &db) noexcept : m_db(db) {}

        // Trả về true nếu toàn vẹn, false nếu có lỗi
        bool checkIntegrity(std::vector<std::string> &messages);

    private:
        SQLiteDB &m_db;
};
