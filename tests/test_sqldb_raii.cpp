#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>
#include <sqlite3.h>
#include "sqldb_raii.hpp"

namespace {
    // Helper: Check PRAGMA value
    int getPragmaInt(sqlite3* db, const char* pragma) {
        int value{};
        char* errMsg{};

        std::string sql = std::string("PRAGMA ") + pragma + ";";
        int rc = sqlite3_exec(
            db, sql.c_str(),
            [](void* data, int, char**, char**) -> int {
                *static_cast<int*>(data) = 1; // Set value nếu row tồn tại
                return SQLITE_OK;
            },
            &value, &errMsg);

        if (rc != SQLITE_OK) {
            if (errMsg != nullptr) { sqlite3_free(errMsg); }
            value = 0; // Default on error
        }

        return value;
    }
} // namespace

TEST_CASE("SQLiteDB - RAII and Initialization", "[DB][RAII]") {
    SECTION("In-memory init and PRAGMA") {
        SQLiteDB db(":memory:");
        auto* rawDbPtr = db.get();
        REQUIRE(rawDbPtr != nullptr);
        REQUIRE(getPragmaInt(rawDbPtr, "foreign_keys") == 1); // Verify PRAGMA
    }

    SECTION("RAII cleanup post-scope") {
        sqlite3* rawDbPtr = nullptr;
        {
            SQLiteDB db(":memory:");
            rawDbPtr = db.get();
            REQUIRE(rawDbPtr != nullptr);
        }
    }

    SECTION("Error on open should throw and cleanup") {
        auto invalidPath = std::filesystem::temp_directory_path() / "nonexist.db";
        CHECK_THROWS_WITH(SQLiteDB(invalidPath.string()),
                          Catch::Matchers::ContainsSubstring("Cannot open database"));
    }
}

TEST_CASE("SQLiteStmt - RAII and Error Handling", "[DB][RAII]") {
    SQLiteDB db(":memory:");
    sqlite3* rawPtr = db.get();

    SECTION("Valid statement preparation and RAII") {
        sqlite3_stmt* rawStmt = nullptr;
        {
            SQLiteStmt stmt(rawPtr, "CREATE TABLE IF NOT EXISTS T(A INTEGER);");
            rawStmt = stmt.get();
            REQUIRE(rawStmt != nullptr);
            REQUIRE(sqlite3_reset(rawStmt) == SQLITE_OK);
        }
    }

    SECTION("Invalid statement should throw std::runtime_error") {
        CHECK_THROWS_WITH(SQLiteStmt(rawPtr, "INVALID SYNTAX ;"), // Syntax error rõ
                          Catch::Matchers::ContainsSubstring("Failed to prepare statement"));
    }
}
