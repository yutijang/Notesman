#include <stdexcept>
#include <string>
#include <vector>
#include <sqlite3.h>

#include "database_checker.hpp"
#include "sqldb_raii.hpp"

bool DatabaseChecker::checkIntegrity(std::vector<std::string> &messages) {
    bool ok{true};

    auto runPragma = [&](const char* sql, const char* label) {
        sqlite3_stmt* stmt{nullptr};
        int rc = sqlite3_prepare_v2(m_db.get(), sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error(std::string("Failed to prepare ") + label + ": " +
                                     sqlite3_errmsg(m_db.get()));
        }

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            const char* textPtr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
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

    runPragma("PRAGMA integrity_check;", "integrity_check");
    runPragma("PRAGMA foreign_key_check;", "foreign_key_check");
    // runPragma("PRAGMA quick_check;", "quick_check");

    return ok;
}

int DatabaseChecker::getDBVersion() {
    int version{};
    sqlite3_stmt* stmt{};

    if (sqlite3_prepare_v2(m_db.get(), "PRAGMA user_version;", -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) { version = sqlite3_column_int(stmt, 0); }
    }

    sqlite3_finalize(stmt);

    return version;
}
