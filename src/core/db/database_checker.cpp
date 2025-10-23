#include <sqlite3.h>
#include <stdexcept>
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
            const unsigned char* text = sqlite3_column_text(stmt, 0);
            if (text) {
                std::string msg(reinterpret_cast<const char*>(text));
                if (msg != "ok") {
                    ok = false;
                    messages.push_back(std::string(label) + ": " + msg);
                }
            }
        }

        sqlite3_finalize(stmt);
    };

    // 1️⃣ Kiểm tra tổng thể
    runPragma("PRAGMA integrity_check;", "integrity_check");

    // 2️⃣ Kiểm tra khóa ngoại (nếu bật foreign_keys)
    runPragma("PRAGMA foreign_key_check;", "foreign_key_check");

    // (tùy chọn) 3️⃣ Kiểm tra nhanh thay thế
    // runPragma("PRAGMA quick_check;", "quick_check");

    return ok;
}
