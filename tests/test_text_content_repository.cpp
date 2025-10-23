#include <stdexcept>
#include <catch2/catch_test_macros.hpp>
#include "sqldb_raii.hpp"
#include "text_content_repository.hpp"

namespace {
    SQLiteDB createInMemoryDB() {
        SQLiteDB db(":memory:");
        sqlite3* rawPtr = db.get();

        const char* schema = R"SQL(
            PRAGMA foreign_keys = ON;

            CREATE TABLE text_content (
                resource_id INTEGER PRIMARY KEY,
                content TEXT
            );

            CREATE VIRTUAL TABLE text_content_fts USING fts5(
                content,
                tokenize = 'unicode61 remove_diacritics 1'
            );

            CREATE TRIGGER text_content_insert_fts
            AFTER INSERT ON text_content
            WHEN new.content IS NOT NULL
            BEGIN
                INSERT INTO text_content_fts (rowid, content)
                VALUES (new.resource_id, new.content);
            END;

            CREATE TRIGGER text_content_update_fts
            AFTER UPDATE ON text_content
            BEGIN
                UPDATE text_content_fts
                SET content = new.content
                WHERE rowid = old.resource_id;
            END;

            CREATE TRIGGER text_content_delete_fts
            AFTER DELETE ON text_content
            BEGIN
                DELETE FROM text_content_fts
                WHERE rowid = old.resource_id;
            END;
        )SQL";

        REQUIRE(sqlite3_exec(rawPtr, schema, nullptr, nullptr, nullptr) == SQLITE_OK);
        return db;
    }
} // namespace

TEST_CASE("TextContentRepository basic CRUD", "[TextContentRepository]") {
    auto db = createInMemoryDB();
    TextContentRepository repo(db);

    SECTION("insert and getTextById") {
        repo.insertText(1, "hello");
        auto text = repo.getTextById(1);

        REQUIRE(text.has_value());
        REQUIRE(*text == "hello");
    }

    SECTION("exists returns true if record present") {
        repo.insertText(42, "Document"); // NOLINT(readability-magic-numbers)

        REQUIRE(repo.exists(42));
        REQUIRE_FALSE(repo.exists(99));
    }

    SECTION("updateText modifies content") {
        repo.insertText(10, "OldText");    // NOLINT(readability-magic-numbers)
        repo.updateText(10, "NewText");    // NOLINT(readability-magic-numbers)

        auto check = repo.getTextById(10); // NOLINT(readability-magic-numbers)

        REQUIRE(check.has_value());
        REQUIRE(*check == "NewText");
    }

    SECTION("updateText throws when id not found") {
        REQUIRE_THROWS_AS(repo.updateText(11, "Text for update"), std::runtime_error);
    }

    SECTION("getAllTexts returns all rows") {
        repo.insertText(50, "John Doe"); // NOLINT(readability-magic-numbers)
        repo.insertText(51, "Jane Doe"); // NOLINT(readability-magic-numbers)

        auto check = repo.getAllTexts();

        REQUIRE(check.size() == 2);
        REQUIRE(check[0].second == "John Doe");
        REQUIRE(check[1].second == "Jane Doe");
    }
}

TEST_CASE("TextContentRepository FTS search", "[TextContentRepository][fts]") {
    auto db = createInMemoryDB();
    TextContentRepository repo(db);

    repo.insertText(1, "C++ is great");
    repo.insertText(2, "Qt is powerful");
    repo.insertText(3, "C++ with Qt");

    SECTION("searchByContentFTS finds matching content") {
        auto results = repo.searchByContentFTS("Qt");

        REQUIRE(results.size() == 2);
        REQUIRE(results[0].second.find("Qt") != std::string::npos);
    }

    SECTION("searchByContentFTS returns empty when no match") {
        auto check = repo.searchByContentFTS("Nothing");

        REQUIRE(check.empty());
    }
}
