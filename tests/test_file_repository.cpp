#include <catch2/catch_test_macros.hpp>
#include <sqlite3.h>
#include "file_repository.hpp"
#include "sqldb_raii.hpp"

namespace {
    SQLiteDB createInMemoryDB() {
        SQLiteDB db(":memory:");
        sqlite3* rawPtr = db.get();

        const char* schema = R"SQL(
            CREATE TABLE files (
                resource_id INTEGER PRIMARY KEY,
                stored_path TEXT,
                original_path TEXT NOT NULL,
                is_managed INTEGER NOT NULL
            );
        )SQL";

        REQUIRE(sqlite3_exec(rawPtr, schema, nullptr, nullptr, nullptr) == SQLITE_OK);

        return db;
    }
} // namespace

TEST_CASE("FileRepository basic insert and get", "[FileRepository]") {
    auto db = createInMemoryDB();
    FileRepository repo(db);

    SECTION("insertFile stores internal managed entry correctly") {
        repo.insertFile(1, "/stored/path/file1.txt", "/original/path/file1.txt", true);

        auto entry = repo.getFileById(1);

        REQUIRE(entry.has_value());
        CHECK(entry->resource_id == 1);
        CHECK(entry->stored_path == "/stored/path/file1.txt");
        CHECK(entry->original_path == "/original/path/file1.txt");
        CHECK(entry->is_managed);
    }

    SECTION("insertFile stores external (unmanaged) entry correctly") {
        repo.insertFile(2, "", "/shared/file2.txt", false);

        auto entry = repo.getFileById(2);

        REQUIRE(entry.has_value());
        CHECK(entry->stored_path == "/shared/file2.txt");
        CHECK(entry->original_path == "/shared/file2.txt");
        CHECK_FALSE(entry->is_managed);
    }

    SECTION("exists() returns true only for existing IDs") {
        repo.insertFile(3, "/store/x", "/orig/x", true);

        CHECK(repo.exists(3));
        CHECK_FALSE(repo.exists(999));
    }

    SECTION("getFileById returns std::nullopt when missing") {
        auto entryOpt = repo.getFileById(42); // NOLINT(readability-magic-numbers)
        REQUIRE_FALSE(entryOpt.has_value());
    }
}

TEST_CASE("FileRepository updateFile modifies existing record", "[FileRepository]") {
    auto db = createInMemoryDB();
    FileRepository repo(db);

    repo.insertFile(1, "/stored/old.txt", "/orig/old.txt", true);

    SECTION("update existing row successfully") {
        repo.updateFile(1, "/stored/new.txt", "/orig/new.txt", false);

        auto updated = repo.getFileById(1);

        REQUIRE(updated.has_value());
        CHECK(updated->stored_path == "/orig/new.txt"); // isManage = false => stored == original
        CHECK(updated->stored_path == "/orig/new.txt");
        CHECK_FALSE(updated->is_managed);
    }

    SECTION("update non-existing row throws runtime_error") {
        REQUIRE_THROWS_AS(repo.updateFile(2, "/path/fileStored.pdf", "/path/file.pdf", true),
                          std::runtime_error);
    }
}

TEST_CASE("FileRepository query by stored/original path", "[FileRepository]") {
    auto db = createInMemoryDB();
    FileRepository repo(db);

    repo.insertFile(1, "/s1", "/o1", true);
    repo.insertFile(2, "/s2", "/o2", false);

    SECTION("getResourceIdBystoredPath finds correct ID") {
        auto resOpt = repo.getResourceIdBystoredPath("/s1");

        REQUIRE(resOpt.has_value());
        CHECK(*resOpt == 1);
    }

    SECTION("getResourceIdByOriginalPath finds correct ID") {
        auto idOpt = repo.getResourceIdByOriginalPath("/o2");

        REQUIRE(idOpt.has_value());
        CHECK(*idOpt == 2);
    }

    SECTION("query returns nullopt for non-existent paths") {
        CHECK_FALSE(repo.getResourceIdBystoredPath("/missing").has_value());
        CHECK_FALSE(repo.getResourceIdByOriginalPath("/none").has_value());
    }
}

TEST_CASE("FileRepository getAllFile returns all rows", "[FileRepository]") {
    auto db = createInMemoryDB();
    FileRepository repo(db);

    repo.insertFile(1, "/s1", "/o1", true);
    repo.insertFile(2, "/s2", "/o2", false);

    auto all = repo.getAllFile();

    REQUIRE(all.size() == 2);

    CHECK(all[0].resource_id == 1);
    CHECK(all[1].resource_id == 2);
}
