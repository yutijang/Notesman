#include "database_checker.hpp"

#include "Logger.hpp"
#include "sqldb_raii.hpp"

#include <cstddef>
#include <exception>
#include <optional>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>

bool DatabaseChecker::checkIntegrity(std::vector<std::string>& messages) {
    bool ok{true};

    auto runPragma = [&](char const* sql, char const* label) {
        sqlite3_stmt* stmt{nullptr};
        int rc = sqlite3_prepare_v2(m_db.get(), sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error(std::string("Failed to prepare ") + label + ": " +
                                     sqlite3_errmsg(m_db.get()));
        }

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            char const* textPtr = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 0));
            int bytes = sqlite3_column_bytes(stmt, 0);

            std::string content;
            if ((textPtr != nullptr) && bytes > 0) {
                content.assign(textPtr, static_cast<std::size_t>(bytes));
            }

            if (content != "ok") {
                ok = false;
                messages.push_back(std::string(label) + ": " + content);
            }
        }

        sqlite3_finalize(stmt);
    };

    try {
        runPragma("PRAGMA integrity_check;", "integrity_check");
        runPragma("PRAGMA foreign_key_check;", "foreign_key_check");
        // runPragma("PRAGMA quick_check;", "quick_check");
    } catch (std::exception const& ex) {
        ok = false;
        messages.emplace_back(ex.what());

        Log::err(ex.what());
    }

    return ok;
}

std::optional<int> DatabaseChecker::getDBVersion() {
    SQLiteStmt stmt(m_db.get(), "PRAGMA user_version;");

    if (stmt.step() != SQLITE_ROW) {
        return std::nullopt;
    }

    return sqlite3_column_int(stmt.get(), 0);
}
