#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <sqlite3.h>
#include <stdexcept>
#include <string>

// RAII wrapper cho sqlite3*
class SQLiteDB {
    public:
        // Custom deleter cho sqlite3*
        struct SqliteDBDeleter {
                void operator()(sqlite3* db) const noexcept { sqlite3_close_v2(db); }
        };

        using unique_sqlite_db_ptr = std::unique_ptr<sqlite3, SqliteDBDeleter>;

        explicit SQLiteDB(std::string const& filename) { open(filename); }

        ~SQLiteDB() = default;
        SQLiteDB(SQLiteDB const&) = delete;      // CẤM COPY để tránh double-free
        SQLiteDB& operator=(SQLiteDB const&) = delete;

        SQLiteDB(SQLiteDB&&) noexcept = default; // CHO PHÉP MOVE
        SQLiteDB& operator=(SQLiteDB&&) noexcept = default;

        [[nodiscard]] explicit operator bool() const noexcept { return m_db != nullptr; }

        [[nodiscard]] sqlite3* get() const noexcept { return m_db.get(); }

        void close() noexcept { m_db.reset(); }

        void open(std::string const& filename) {
            sqlite3* dbPtr = nullptr;

            int rc = sqlite3_open_v2(filename.c_str(), &dbPtr,
                                     SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI,
                                     nullptr);
            if (rc != SQLITE_OK) {
                std::string errorMSG = (dbPtr != nullptr) ? sqlite3_errmsg(dbPtr) : "unknown";
                if (dbPtr != nullptr) { sqlite3_close_v2(dbPtr); }

                throw std::runtime_error("Cannot open database: " + errorMSG);
            }

            // Busy timeout: 2000ms — cho phép writer khác hoàn thành trước khi fail
            sqlite3_busy_timeout(dbPtr, 2000); // NOLINT(readability-magic-numbers)

            // =======================================================
            // Bổ sung: Kích hoạt Foreign Keys (Best Practice)
            // WAL mode: nhiều reader + 1 writer đồng thời, không block lẫn nhau
            // synchronous=NORMAL: an toàn với WAL, hiệu năng tốt hơn FULL
            // =======================================================
            char const* pragmaInit = "PRAGMA journal_mode=WAL;"
                                     "PRAGMA synchronous=NORMAL;"
                                     "PRAGMA foreign_keys=ON;";
            rc = sqlite3_exec(dbPtr, pragmaInit, nullptr, nullptr, nullptr);

            if (rc != SQLITE_OK) {
                // Xử lý lỗi: Nếu không thể bật PRAGMA, cần đóng DB và báo lỗi
                std::string errorMSG = sqlite3_errmsg(dbPtr);
                sqlite3_close_v2(dbPtr);
                throw std::runtime_error("Failed to enable PRAGMA foreign_keys: " + errorMSG);
            }

            m_db.reset(dbPtr);
            m_filename = filename;
        }

        [[nodiscard]] std::string const& getFilename() const noexcept { return m_filename; }

    private:
        unique_sqlite_db_ptr m_db;
        std::string m_filename;
};

// RAII wrapper cho sqlite3_stmt*
class SQLiteStmt {
    public:
        struct SqliteStmtDeleter {
                void operator()(sqlite3_stmt* stmt) const noexcept { sqlite3_finalize(stmt); }
        };

        using unique_sqlite_stmt_ptr = std::unique_ptr<sqlite3_stmt, SqliteStmtDeleter>;

        explicit SQLiteStmt(sqlite3* db, std::string const& query) {
            if (db == nullptr) { throw std::invalid_argument("SQLiteStmt: db is null"); }

            sqlite3_stmt* stmtPtr = nullptr;

            int rc = sqlite3_prepare_v2(db, query.c_str(), -1, &stmtPtr, nullptr);

            if (rc != SQLITE_OK) {
                std::string errorMSG = sqlite3_errmsg(db);
                throw std::runtime_error("Failed to prepare statement: " + errorMSG);
            }

            m_stmt.reset(stmtPtr);
        }

        // Rule of Five
        ~SQLiteStmt() = default;

        SQLiteStmt(SQLiteStmt const&) = delete;
        SQLiteStmt& operator=(SQLiteStmt const&) = delete;

        SQLiteStmt(SQLiteStmt&&) noexcept = default;
        SQLiteStmt& operator=(SQLiteStmt&&) noexcept = default;

        // State check
        [[nodiscard]] explicit operator bool() const noexcept { return m_stmt != nullptr; }

        [[nodiscard]] sqlite3_stmt* get() const noexcept { return m_stmt.get(); }

        // Reset statement (reuse)
        void reset() noexcept {
            if (m_stmt) { sqlite3_reset(m_stmt.get()); }
        }

        // Clear bound parameters
        void clearBindings() noexcept {
            if (m_stmt) { sqlite3_clear_bindings(m_stmt.get()); }
        }

        [[nodiscard]] int step() noexcept {
            assert(m_stmt);
            return sqlite3_step(m_stmt.get());
        }

        [[nodiscard]] std::string getColumnText(int index) const {
            char const* text =
                reinterpret_cast<char const*>(sqlite3_column_text(m_stmt.get(), index));
            if (text == nullptr) { return {}; }

            int bytes = sqlite3_column_bytes(m_stmt.get(), index);
            return {text, static_cast<std::size_t>(bytes)};
        }

        [[nodiscard]] sqlite3_int64 getColumnInt64(int index) const noexcept {
            return sqlite3_column_int64(m_stmt.get(), index);
        }

    private:
        unique_sqlite_stmt_ptr m_stmt;
};
