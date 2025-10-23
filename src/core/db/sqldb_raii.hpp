#pragma once

#include <string>
#include <stdexcept>
#include <memory>
#include <sqlite3.h>

// RAII wrapper cho sqlite3*
class SQLiteDB {
    public:
        // Custom deleter cho sqlite3*
        struct SqliteDBDeleter {
                void operator()(sqlite3* db) const { sqlite3_close_v2(db); }
        };

        using unique_sqlite_db_ptr = std::unique_ptr<sqlite3, SqliteDBDeleter>;

        explicit SQLiteDB(const std::string &filename) {
            sqlite3* dbPtr = nullptr;

            int rc = sqlite3_open_v2(filename.c_str(), &dbPtr,
                                     SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI, nullptr);
            if (rc != SQLITE_OK) {
                std::string errorMSG = (dbPtr != nullptr) ? sqlite3_errmsg(dbPtr) : "unknown";
                if (dbPtr != nullptr) { sqlite3_close_v2(dbPtr); }

                throw std::runtime_error("Cannot open database: " + errorMSG);
            }

            // =======================================================
            // Bổ sung: Kích hoạt Foreign Keys (Best Practice)
            // =======================================================
            const char* pragmaFKON = "PRAGMA foreign_keys = ON;";
            rc = sqlite3_exec(dbPtr, pragmaFKON, nullptr, nullptr, nullptr);

            if (rc != SQLITE_OK) {
                // Xử lý lỗi: Nếu không thể bật PRAGMA, cần đóng DB và báo lỗi
                std::string errorMSG = sqlite3_errmsg(dbPtr);
                sqlite3_close_v2(dbPtr);
                throw std::runtime_error("Failed to enable PRAGMA foreign_keys: " + errorMSG);
            }

            m_db = unique_sqlite_db_ptr(dbPtr);
        }

        [[nodiscard]] sqlite3* get() const noexcept { return m_db.get(); }

    private:
        unique_sqlite_db_ptr m_db;
};

// RAII wrapper cho sqlite3_stmt*
class SQLiteStmt {
    public:
        struct SqliteStmtDeleter {
                void operator()(sqlite3_stmt* stmt) const noexcept { sqlite3_finalize(stmt); }
        };

        using unique_sqlite_stmt_ptr = std::unique_ptr<sqlite3_stmt, SqliteStmtDeleter>;

        explicit SQLiteStmt(sqlite3* db, const std::string &query) {
            sqlite3_stmt* stmtPtr = nullptr;

            int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmtPtr, nullptr);

            if (rc != SQLITE_OK) {
                std::string errorMSG = sqlite3_errmsg(db);
                throw std::runtime_error("Failed to prepare statement: " + errorMSG);
            }
            m_stmt.reset(stmtPtr);
        }

        [[nodiscard]] sqlite3_stmt* get() const noexcept { return m_stmt.get(); }

    private:
        unique_sqlite_stmt_ptr m_stmt;
};

#pragma region
/*
// RAII wrapper cho sqlite3*
class SQLiteDB {
    public:
        explicit SQLiteDB(const std::string &filename) {
            int rc = sqlite3_open_v2(filename.c_str(), &m_db,
                                     SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI, nullptr);
            if (rc != SQLITE_OK) {
                sqlite3_close_v2(m_db); // nên đóng ở đây khi lỗi
                throw std::runtime_error("Cannot open database: " +
                                         std::string(sqlite3_errmsg(m_db)));
            }
        }

        ~SQLiteDB() { sqlite3_close_v2(m_db); }         // Tự động đóng database

        SQLiteDB(const SQLiteDB &) = delete;            // Không cho phép copy
        SQLiteDB &operator=(const SQLiteDB &) = delete; // Không cho phép copy assignment
operator

        SQLiteDB(SQLiteDB &&other) noexcept : m_db(other.m_db) { other.m_db = nullptr; }

        SQLiteDB &operator=(SQLiteDB &&other) noexcept {
            if (this != &other) {
                sqlite3_close_v2(m_db);
                m_db = other.m_db;
                other.m_db = nullptr;
            }
            return *this;
        }

        [[nodiscard]] sqlite3* get() const noexcept { return m_db; }

    private:
        sqlite3* m_db{nullptr};
};
*/
#pragma endregion
/*
// RAII wrapper cho sqlite3_stmt*
class SQLiteStmt {
    public:
        explicit SQLiteStmt(sqlite3* db, const std::string &query) {
            int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &m_stmt, nullptr);
            if (rc != SQLITE_OK) {
                throw std::runtime_error("Failed to prepare statement: " +
                                         std::string(sqlite3_errmsg(db)));
            }
        }

        ~SQLiteStmt() { sqlite3_finalize(m_stmt); } // Tự động giải phóng statement

        SQLiteStmt(const SQLiteStmt &) = delete;    // Không cho phép copy
        SQLiteStmt &
            operator=(const SQLiteStmt &) = delete; // Không cho phép copy assignment
operator

        SQLiteStmt(SQLiteStmt &&other) noexcept : m_stmt(other.m_stmt) { other.m_stmt =
nullptr; }

        SQLiteStmt &operator=(SQLiteStmt &&other) noexcept {
            if (this != &other) {
                sqlite3_finalize(m_stmt);
                m_stmt = other.m_stmt;
                other.m_stmt = nullptr;
            }
            return *this;
        }

        [[nodiscard]] sqlite3_stmt* get() const noexcept { return m_stmt; }

    private:
        sqlite3_stmt* m_stmt{nullptr};
};

*/