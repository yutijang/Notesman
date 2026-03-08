#pragma once

#include "sqldb_raii.hpp"

#include <optional>
#include <sqlite3.h>
#include <string>
#include <vector>

class DatabaseChecker {
    public:
        explicit DatabaseChecker(SQLiteDB& db) noexcept : m_db(db) {}

        // Trả về true nếu toàn vẹn, false nếu có lỗi
        bool checkIntegrity(std::vector<std::string>& messages);

        std::optional<int> getDBVersion();

    private:
        SQLiteDB& m_db;
};
