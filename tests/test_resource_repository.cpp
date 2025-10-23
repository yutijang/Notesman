#include <utility>
#include <catch2/catch_test_macros.hpp>
#include <sqlite3.h>
#include "model.hpp"
#include "resource_repository.hpp"
#include "sqldb_raii.hpp"

namespace {
    SQLiteDB createInMemoryDB() {
        SQLiteDB db(":memory:");
        sqlite3* rawPtr = db.get();

        const char* schema = R"SQL(
            PRAGMA foreign_keys = ON;

            CREATE TABLE resources (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                title TEXT NOT NULL,
                type TEXT NOT NULL,
                file_hash TEXT,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                updated_at TEXT DEFAULT CURRENT_TIMESTAMP
            );

            CREATE VIRTUAL TABLE resources_fts USING fts5(
                title,
                tokenize = 'unicode61 remove_diacritics 1'
            );

            CREATE TRIGGER resources_insert_fts
            AFTER INSERT ON resources
            WHEN new.title IS NOT NULL
            BEGIN
                INSERT INTO resources_fts(rowid, title)
                VALUES (new.id, new.title);
            END;

            CREATE TRIGGER resources_update_fts
            AFTER UPDATE ON resources
            BEGIN
                UPDATE resources_fts
                SET title = new.title
                WHERE rowid = old.id;
            END;

            CREATE TRIGGER resources_delete_fts
            AFTER DELETE ON resources
            BEGIN
                DELETE FROM resources_fts
                WHERE rowid = old.id;
            END;
        )SQL";

        REQUIRE(sqlite3_exec(rawPtr, schema, nullptr, nullptr, nullptr) == SQLITE_OK);

        return db;
    }

    Resource makeResource(std::string title, ResourceType type, std::string fileHash = "") {
        Resource r{};
        r.title = std::move(title);
        r.type = type;
        r.file_hash = std::move(fileHash);

        return r;
    }
} // namespace

TEST_CASE("ResourceRepository basic CRUD", "[ResourceRepository]") {
    auto db = createInMemoryDB();
    ResourceRepository repo(db);

    SECTION("insert and getById") {
        Resource res = makeResource("Doc1", ResourceType::cpp, "hash123");
        auto id = repo.insert(res);

        auto resOpt = repo.getById(id);
        REQUIRE(resOpt.has_value());
        REQUIRE(resOpt->title == "Doc1");
        REQUIRE(resOpt->type == ResourceType::cpp);
    }

    SECTION("update resource title") {
        Resource res = makeResource("OldTitle", ResourceType::text);
        auto id = repo.insert(res);

        res.id = id;
        res.title = "NewTitle";
        repo.update(res);

        auto resOpt = repo.getById(id);
        REQUIRE(resOpt.has_value());
        REQUIRE(resOpt->title == "NewTitle");
    }

    SECTION("remove deletes resource") {
        Resource res = makeResource("Doc2", ResourceType::text);
        auto id = repo.insert(res);
        repo.remove(id);

        auto resOpt = repo.getById(id);
        REQUIRE_FALSE(resOpt.has_value());
    }
}

TEST_CASE("ResourceRepository utility functions", "[ResourceRepository]") {
    auto db = createInMemoryDB();
    ResourceRepository repo(db);

    Resource a = makeResource("A", ResourceType::text);
    Resource b = makeResource("B", ResourceType::pdf, "hashString");
    repo.insert(a);
    repo.insert(b);

    SECTION("existsTitle returns true if exists") {
        REQUIRE(repo.existsTitle("A", ResourceType::text));
        REQUIRE_FALSE(repo.existsTitle("NonExistTitle", ResourceType::text));
    }

    SECTION("getByFileHash returns correct resource") {
        auto resOpt = repo.getByFileHash("hashString");
        REQUIRE(resOpt.has_value());
        REQUIRE(resOpt->title == "B");
    }

    SECTION("updateFileHash updates value correctly") {
        auto resOpt = repo.getByFileHash("hashString");
        REQUIRE(resOpt.has_value());
        auto id = resOpt->id;

        repo.updateFileHash(id, "newHash");
        auto updated = repo.getByFileHash("newHash");
        REQUIRE(updated.has_value());
        REQUIRE(updated->title == "B");
        REQUIRE(updated->id == id);
    }

    SECTION("searchByTitleFTS") {
        auto resOpt = repo.searchByTitleFTS("A");
        REQUIRE_FALSE(resOpt.empty());
    }
}
