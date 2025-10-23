#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "file_repository.hpp"
#include "file_service.hpp"
#include "model.hpp"
#include "resource_repository.hpp"
#include "resource_service.hpp"
#include "sqldb_raii.hpp"
#include "tag_repository.hpp"
#include "text_content_repository.hpp"

namespace {
    void createMinimalSchema(sqlite3* db) {
        const char* schema = R"SQL(
        -- =====================================================
        -- Core tables
        -- =====================================================
        CREATE TABLE resources (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            type TEXT NOT NULL,
            file_hash TEXT,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        );

        CREATE VIRTUAL TABLE resources_fts
            USING fts5(title, content='resources', content_rowid='id');

        CREATE TABLE files (
            resource_id INTEGER PRIMARY KEY,
            stored_path TEXT,
            original_path TEXT,
            is_managed INTEGER
        );

        CREATE TABLE text_content (
            resource_id INTEGER PRIMARY KEY,
            content TEXT
        );

        CREATE VIRTUAL TABLE text_content_fts
            USING fts5(content, content='text_content', content_rowid='resource_id');

        -- =====================================================
        -- Tagging system
        -- =====================================================
        CREATE TABLE tags (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE
        );

        CREATE TABLE resource_tags (
            resource_id INTEGER NOT NULL,
            tag_id INTEGER NOT NULL,
            PRIMARY KEY (resource_id, tag_id)
        );
    )SQL";

        REQUIRE(sqlite3_exec(db, schema, nullptr, nullptr, nullptr) == SQLITE_OK);
    }
} // namespace

// ======================================================================
// ========== TESTS =====================================================
// ======================================================================

TEST_CASE("ResourceService addTextResource basic behavior", "[ResourceService]") {
    SQLiteDB db(":memory:");
    createMinimalSchema(db.get());

    ResourceRepository resRepo(db);
    FileRepository fileRepo(db);
    TextContentRepository textRepo(db);
    TagRepository tagRepo(db);
    FileService fileService(db, fileRepo, resRepo);

    ResourceService service(db, resRepo, fileRepo, textRepo, tagRepo, fileService);

    SECTION("throws if not ResourceType::text") {
        REQUIRE_THROWS_AS(service.addTextResource("Doc", "Body", ResourceType::pdf),
                          std::runtime_error);
    }

    SECTION("calls insert on repos when type is text") {
        auto id = service.addTextResource("Doc", "Body", ResourceType::text);
        auto res = resRepo.getById(id);
        REQUIRE(res.has_value());
        CHECK(res->title == "Doc");

        auto content = textRepo.getTextById(id);
        REQUIRE(content.has_value());
        CHECK(*content == "Body");
    }
}

TEST_CASE("ResourceService addFileResource delegates correctly", "[ResourceService]") {
    SQLiteDB db(":memory:");
    createMinimalSchema(db.get());

    ResourceRepository resRepo(db);
    FileRepository fileRepo(db);
    TextContentRepository textRepo(db);
    TagRepository tagRepo(db);
    FileService fileService(db, fileRepo, resRepo);

    ResourceService service(db, resRepo, fileRepo, textRepo, tagRepo, fileService);

    std::filesystem::path tmp = std::filesystem::temp_directory_path() / "rs_test.txt";
    std::ofstream(tmp) << "temp";

    auto id = service.addFileResource(tmp.string(), "File1", ResourceType::pdf, true);
    auto file = fileRepo.getFileById(id);
    REQUIRE(file.has_value());
    CHECK(file->is_managed);
    CHECK(file->original_path.find("rs_test.txt") != std::string::npos);
}

TEST_CASE("ResourceService getFullResource combines repositories correctly", "[ResourceService]") {
    SQLiteDB db(":memory:");
    createMinimalSchema(db.get());

    sqlite3_exec(db.get(),
                 "INSERT INTO resources (title, type) VALUES ('Doc1', 'text');"
                 "INSERT INTO text_content VALUES (1, 'hello');"
                 "INSERT INTO tags (name) VALUES ('qt');"
                 "INSERT INTO resource_tags VALUES (1, 1);",
                 nullptr, nullptr, nullptr);

    ResourceRepository resRepo(db);
    FileRepository fileRepo(db);
    TextContentRepository textRepo(db);
    TagRepository tagRepo(db);
    FileService fileService(db, fileRepo, resRepo);

    ResourceService service(db, resRepo, fileRepo, textRepo, tagRepo, fileService);

    auto result = service.getFullResource(1);
    REQUIRE(result.has_value());
    CHECK(result->content.has_value());
    CHECK(result->tags.size() == 1);
    CHECK(result->tags[0] == "qt");
}

TEST_CASE("ResourceService deleteResource removes managed files", "[ResourceService]") {
    SQLiteDB db(":memory:");
    createMinimalSchema(db.get());

    ResourceRepository resRepo(db);
    FileRepository fileRepo(db);
    TextContentRepository textRepo(db);
    TagRepository tagRepo(db);
    FileService fileService(db, fileRepo, resRepo);

    std::filesystem::path tmpFile = std::filesystem::temp_directory_path() / "delete_rs_test.txt";
    std::ofstream(tmpFile) << "dummy";

    sqlite3_stmt* stmt{};
    sqlite3_prepare_v2(db.get(), "INSERT INTO resources (title, type) VALUES ('F', 'pdf');", -1,
                       &stmt, nullptr);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_exec(db.get(),
                 ("INSERT INTO files VALUES (1, '" + tmpFile.string() + "', 'orig', 1);").c_str(),
                 nullptr, nullptr, nullptr);

    REQUIRE(std::filesystem::exists(tmpFile));
    ResourceService service(db, resRepo, fileRepo, textRepo, tagRepo, fileService);
    service.deleteResource(1);
    CHECK_FALSE(std::filesystem::exists(tmpFile));
}

TEST_CASE("ResourceService addTagToResource handles existing/new tags correctly",
          "[ResourceService]") {
    SQLiteDB db(":memory:");
    createMinimalSchema(db.get());

    ResourceRepository resRepo(db);
    FileRepository fileRepo(db);
    TextContentRepository textRepo(db);
    TagRepository tagRepo(db);
    FileService fileService(db, fileRepo, resRepo);

    sqlite3_exec(db.get(),
                 "INSERT INTO resources (title, type) VALUES ('Doc', 'text');"
                 "INSERT INTO tags (name) VALUES ('qt');",
                 nullptr, nullptr, nullptr);

    ResourceService service(db, resRepo, fileRepo, textRepo, tagRepo, fileService);

    SECTION("links existing tag") {
        REQUIRE_NOTHROW(service.addTagToResource(1, "qt"));
    }

    SECTION("adds new tag when not exist") {
        REQUIRE_NOTHROW(service.addTagToResource(1, "newtag"));
    }

    SECTION("throws when addTag() fails") {
        // khóa bảng tags để giả lập lỗi insert
        sqlite3_exec(db.get(), "DROP TABLE tags;", nullptr, nullptr, nullptr);
        REQUIRE_THROWS_AS(service.addTagToResource(1, "failtag"), std::runtime_error);
    }
}

TEST_CASE("ResourceService isExistTitle delegates correctly", "[ResourceService]") {
    SQLiteDB db(":memory:");
    createMinimalSchema(db.get());

    sqlite3_exec(db.get(), "INSERT INTO resources (title, type) VALUES ('abc', 'pdf');", nullptr,
                 nullptr, nullptr);

    ResourceRepository resRepo(db);
    FileRepository fileRepo(db);
    TextContentRepository textRepo(db);
    TagRepository tagRepo(db);
    FileService fileService(db, fileRepo, resRepo);

    ResourceService service(db, resRepo, fileRepo, textRepo, tagRepo, fileService);
    CHECK(service.isExistTitle("abc", ResourceType::pdf));
}

TEST_CASE("ResourceService searchByTitleFull aggregates results", "[ResourceService]") {
    SQLiteDB db(":memory:");
    createMinimalSchema(db.get());

    sqlite3_exec(db.get(),
                 "INSERT INTO resources (title, type) VALUES "
                 "('Learn Qt', 'text'),"
                 "('Qt GUI', 'text');"
                 "INSERT INTO text_content VALUES (1, 'intro c++');"
                 "INSERT INTO text_content VALUES (2, 'qt tutorial');"
                 "INSERT INTO tags (name) VALUES ('cpp'), ('qt');"
                 "INSERT INTO resource_tags VALUES (1, 1);"
                 "INSERT INTO resource_tags VALUES (2, 2);"
                 "INSERT INTO resources_fts(rowid, title) VALUES (1, 'Qt C++');"
                 "INSERT INTO resources_fts(rowid, title) VALUES (2, 'Qt GUI');",
                 nullptr, nullptr, nullptr);

    ResourceRepository resRepo(db);
    FileRepository fileRepo(db);
    TextContentRepository textRepo(db);
    TagRepository tagRepo(db);
    FileService fileService(db, fileRepo, resRepo);

    ResourceService service(db, resRepo, fileRepo, textRepo, tagRepo, fileService);
    auto result = service.searchByTitleFull("Qt");

    REQUIRE(result.size() == 2);
    CHECK(result[1].tags[0] == "qt");
}

TEST_CASE("ResourceService searchByContentFull aggregates results", "[ResourceService]") {
    SQLiteDB db(":memory:");
    createMinimalSchema(db.get());

    sqlite3_exec(db.get(),
                 "INSERT INTO resources (title, type) VALUES "
                 "('Doc1', 'text'),"
                 "('Doc2', 'text');"
                 "INSERT INTO text_content VALUES (1, 'C plus plus intro');"
                 "INSERT INTO text_content VALUES (2, 'Qt advanced plus');"
                 "INSERT INTO tags (name) VALUES ('cpp'), ('qt');"
                 "INSERT INTO resource_tags VALUES (1, 1);"
                 "INSERT INTO resource_tags VALUES (2, 2);"
                 "INSERT INTO text_content_fts(rowid, content) "
                 "SELECT resource_id, content FROM text_content;",
                 nullptr, nullptr, nullptr);

    ResourceRepository resRepo(db);
    FileRepository fileRepo(db);
    TextContentRepository textRepo(db);
    TagRepository tagRepo(db);
    FileService fileService(db, fileRepo, resRepo);

    ResourceService service(db, resRepo, fileRepo, textRepo, tagRepo, fileService);

    auto results = service.searchByContentFull("plus");
    REQUIRE(results.size() == 2);
    CHECK(results[0].resource.title == "Doc1");
}
