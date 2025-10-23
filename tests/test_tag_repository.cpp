#include <algorithm>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <sqlite3.h>
#include "model.hpp"
#include "sqldb_raii.hpp"
#include "tag_repository.hpp"

namespace {
    SQLiteDB createInMemoryDB() {
        SQLiteDB db(":memory:");
        sqlite3* rawPtr = db.get();

        const char* schema = R"SQL(
            PRAGMA foreign_keys = ON;

            CREATE TABLE resources (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                title       TEXT NOT NULL,
                type        TEXT NOT NULL,
                file_hash   TEXT UNIQUE NULL,
                created_at  TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                updated_at  TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
                UNIQUE (title, type)
            );

            CREATE TABLE tags (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                name        TEXT UNIQUE NOT NULL COLLATE NOCASE
            );

            CREATE TABLE resource_tags (
                resource_id INTEGER,
                tag_id      INTEGER,
                PRIMARY KEY (resource_id, tag_id),
                FOREIGN KEY (resource_id) REFERENCES resources(id) ON DELETE CASCADE,
                FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
            );
        )SQL";

        REQUIRE(sqlite3_exec(rawPtr, schema, nullptr, nullptr, nullptr) == SQLITE_OK);

        return db;
    }

    Resource makeResource(const std::string &title, ResourceType type = ResourceType::text) {
        Resource r{};
        r.title = title;
        r.type = type;
        return r;
    }

    sqlite3_int64 insertResource(SQLiteDB &db, const Resource &res) {
        SQLiteStmt stmt(db.get(), "INSERT INTO resources (title, type) VALUES (?, ?);");

        sqlite3_bind_text(stmt.get(), 1, res.title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, resourceTypeToString(res.type), -1, SQLITE_TRANSIENT);

        REQUIRE(sqlite3_step(stmt.get()) == SQLITE_DONE);

        return sqlite3_last_insert_rowid(db.get());
    }
} // namespace

TEST_CASE("TagRepository basic tag operations", "[TagRepository]") {
    auto db = createInMemoryDB();
    TagRepository repo(db);

    SECTION("addTag inserts a new tag and returns ID") {
        auto idOpt = repo.addTag("cpp");

        REQUIRE(idOpt.has_value());
        REQUIRE(*idOpt > 0);
    }

    SECTION("addTag on duplicate returns existing ID") {
        auto idOpt1 = repo.addTag("learn");
        auto idOpt2 = repo.addTag("learn");

        REQUIRE(idOpt1.has_value());
        REQUIRE(idOpt2.has_value());
        REQUIRE(*idOpt1 == *idOpt2);
    }

    SECTION("addTags inserts multiple unique tags") {
        std::vector<std::string> name{"one", "two", "three"};
        auto addAgs = repo.addTags(name);

        REQUIRE(addAgs.size() == 3);
        REQUIRE(addAgs[0] != addAgs[1]);
    }

    SECTION("getTagIdByName returns nullopt if tag not exist") {
        auto id = repo.getTagIdByName("nonexistent");

        REQUIRE_FALSE(id.has_value());
    }
}

TEST_CASE("TagRepository link resource and tags", "[TagRepository][link]") {
    auto db = createInMemoryDB();
    TagRepository repo(db);

    Resource res = makeResource("Note 1");
    sqlite3_int64 resId = insertResource(db, res);

    auto tagIdOpt = repo.addTag("cpp");

    REQUIRE(tagIdOpt.has_value());

    SECTION("linkResourceIdWithTag inserts into resource_tags") {
        repo.linkResourceIdWithTag({.resourceId = resId, .tagId = *tagIdOpt});

        SQLiteStmt stmt(db.get(), "SELECT COUNT(*) FROM resource_tags;");

        REQUIRE(sqlite3_step(stmt.get()) == SQLITE_ROW);
        REQUIRE(sqlite3_column_int64(stmt.get(), 0) == 1);
    }

    SECTION("linkResourceWithTags creates multiple links") {
        repo.linkResourceWithTags(resId, {"hpp", "gui", "qt"});
        SQLiteStmt stmt(db.get(), "SELECT COUNT(*) FROM resource_tags;");
        REQUIRE(sqlite3_step(stmt.get()) == SQLITE_ROW);
        REQUIRE(sqlite3_column_int(stmt.get(), 0) == 3);
    }
}

TEST_CASE("TagRepository query tags and resources", "[TagRepository][query]") {
    auto db = createInMemoryDB();
    TagRepository repo(db);

    auto resA = insertResource(db, makeResource("Doc A"));
    auto resB = insertResource(db, makeResource("Doc B"));

    repo.linkResourceWithTags(resA, {"qt", "c++"});
    repo.linkResourceWithTags(resB, {"qt"});

    SECTION("getTagsByResourceId returns correct tags") {
        auto tags = repo.getTagsByResourceId(resA);

        REQUIRE(tags.size() == 2);
        auto foundQt = std::ranges::any_of(tags, [](const auto &t) { return t.second == "qt"; });
        REQUIRE(foundQt);
    }

    SECTION("getAllTags returns all unique tags") {
        auto tags = repo.getAllTags();

        REQUIRE(tags.size() == 2);
    }

    SECTION("getResourcesViaOneTag returns matching resources") {
        auto res = repo.getResourcesViaOneTag("qt");

        REQUIRE(res.size() == 2);
        REQUIRE(res[0].title.find("Doc") != std::string::npos);
    }

    SECTION("getResourcesViaTags (AND logic) returns only matching resources") {
        auto result = repo.getResourcesViaTags({"qt", "c++"});

        REQUIRE(result.size() == 1);
        REQUIRE(result[0].title == "Doc A");
    }
}

TEST_CASE("TagRepository delete operations", "[TagRepository][delete]") {
    auto db = createInMemoryDB();
    TagRepository repo(db);

    auto resId = insertResource(db, makeResource("NoteToDelete"));
    repo.linkResourceWithTags(resId, {"qt", "sqlite"});

    auto tagsBefore = repo.getTagsByResourceId(resId);
    REQUIRE(tagsBefore.size() == 2);

    SECTION("deleteTagFromResource removes a specific link") {
        auto tagId = repo.getTagIdByName("qt");
        REQUIRE(tagId.has_value());

        repo.deleteTagFromResource({.resourceId = resId, .tagId = *tagId});

        auto tagsAfter = repo.getTagsByResourceId(resId);
        REQUIRE(tagsAfter.size() == 1);
        REQUIRE(tagsAfter[0].second == "sqlite");
    }

    SECTION("deleteAllTagsFromResource removes all tags") {
        repo.deleteAllTagsFromResource(resId);

        auto tagsAfter = repo.getTagsByResourceId(resId);
        REQUIRE(tagsAfter.empty());
    }

    SECTION("ON DELETE CASCADE removes from resource_tags when resource deleted") {
        SQLiteStmt del(db.get(), "DELETE FROM resources WHERE id = ?;");
        sqlite3_bind_int64(del.get(), 1, resId);
        REQUIRE(sqlite3_step(del.get()) == SQLITE_DONE);

        SQLiteStmt check(db.get(), "SELECT COUNT(*) FROM resource_tags;");
        REQUIRE(sqlite3_step(check.get()) == SQLITE_ROW);
        REQUIRE(sqlite3_column_int64(check.get(), 0) == 0);
    }
}
