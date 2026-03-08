#include "file_repository.hpp"
#include "file_service.hpp"
#include "file_text_content_repository.hpp"
#include "model.hpp"
#include "resource_repository.hpp"
#include "sqldb_raii.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>

namespace {
    void createMinimalFileServiceSchema(sqlite3* db) {
        char const* schema = R"SQL(
                -- resources table (thêm created_at/updated_at vì repo/service có dùng)
                CREATE TABLE resources (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    title TEXT NOT NULL,
                    type TEXT NOT NULL,
                    file_hash TEXT,
                    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                    updated_at TEXT DEFAULT CURRENT_TIMESTAMP
                );

                -- files table: dùng cả original_path và stored_path (tên thường thấy trong mã)
                CREATE TABLE files (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    resource_id INTEGER NOT NULL,
                    original_path TEXT,       -- đường dẫn gốc người dùng
                    stored_path TEXT,         -- đường dẫn lưu trữ nội bộ
                    is_managed INTEGER DEFAULT 0,
                    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                    FOREIGN KEY(resource_id) REFERENCES resources(id)
                );

                -- text_content / fts nếu cần ở các test khác (không gây hại nếu không dùng)
                CREATE TABLE IF NOT EXISTS text_content (
                    resource_id INTEGER PRIMARY KEY,
                    content TEXT
                );
                CREATE VIRTUAL TABLE IF NOT EXISTS text_content_fts
                    USING fts5(content, content='text_content', content_rowid='resource_id');
            )SQL";

        REQUIRE(sqlite3_exec(db, schema, nullptr, nullptr, nullptr) == SQLITE_OK);
    }

    std::filesystem::path createTempFile(std::string const& name, std::string_view content) {
        auto path = std::filesystem::temp_directory_path() / name;
        std::ofstream ofs(path);
        ofs << content;
        return path;
    }

} // namespace

// ------------------------------------------------------------
// Test group: computeFileHash
// ------------------------------------------------------------
TEST_CASE("FileService::computeFileHash produces deterministic value", "[FileService]") {
    SQLiteDB db(":memory:");
    createMinimalFileServiceSchema(db.get());

    FileRepository fileRepo(db);
    ResourceRepository resRepo(db);
    FileTextContentRepository fileTextRepo(db);
    FileService service(fileRepo, resRepo, fileTextRepo);

    auto file = createTempFile("hash.txt", "hello");
    auto hash1 = FileService::computeFileHash(file);
    auto hash2 = FileService::computeFileHash(file);

    CHECK(hash1 == hash2); // cùng nội dung → hash trùng

    auto diffFile = createTempFile("hash2.txt", "world");
    auto hash3 = FileService::computeFileHash(diffFile);
    CHECK(hash1 != hash3);

    std::filesystem::remove(file);
    std::filesystem::remove(diffFile);
}

// ------------------------------------------------------------
// Test group: addFileResource (thêm và tái sử dụng)
// ------------------------------------------------------------
TEST_CASE("FileService::addFileResource inserts and reuses existing resource", "[FileService]") {
    SQLiteDB db(":memory:");
    createMinimalFileServiceSchema(db.get());

    FileRepository fileRepo(db);
    ResourceRepository resRepo(db);
    FileTextContentRepository fileTextRepo(db);
    FileService service(fileRepo, resRepo, fileTextRepo);

    // file đầu tiên → thêm mới
    auto file1 = createTempFile("file1.txt", "abc");
    auto id1 = service.addFileResource(file1.string(), "Doc1", ResourceType::PdfDoc, false, "");
    CHECK(id1 == 1);

    // file khác nhưng cùng nội dung → cùng hash → tái sử dụng
    auto file2 = createTempFile("file2.txt", "abc");
    auto id2 = service.addFileResource(file2.string(), "Doc2", ResourceType::PdfDoc, false, "");
    CHECK(id2 == id1);

    // file khác nội dung → hash khác → tạo mới
    auto file3 = createTempFile("file3.txt", "xyz");
    auto id3 = service.addFileResource(file3.string(), "Doc3", ResourceType::PdfDoc, false, "");
    CHECK(id3 != id1);

    std::filesystem::remove(file1);
    std::filesystem::remove(file2);
    std::filesystem::remove(file3);
}

// ------------------------------------------------------------
// Test group: findResourceByFile
// ------------------------------------------------------------
TEST_CASE("FileService::findResourceByFile finds by existing hash", "[FileService]") {
    SQLiteDB db(":memory:");
    createMinimalFileServiceSchema(db.get());

    FileRepository fileRepo(db);
    ResourceRepository resRepo(db);
    FileTextContentRepository fileTextRepo(db);
    FileService service(fileRepo, resRepo, fileTextRepo);

    auto file = createTempFile("find.txt", "abc");
    auto hash = FileService::computeFileHash(file);

    // chèn resource có file_hash = hash
    sqlite3_exec(
        db.get(),
        ("INSERT INTO resources (title, type, file_hash) VALUES ('A','pdf','" + hash + "');")
            .c_str(),
        nullptr, nullptr, nullptr);

    // gọi hàm: trả về optional<sqlite3_int64>
    auto idOpt = service.findResourceByFile(file.string());
    REQUIRE(idOpt.has_value());

    // xác nhận resource thực sự tồn tại và có title mong muốn
    auto resOpt = resRepo.getById(*idOpt);
    REQUIRE(resOpt.has_value());
    CHECK(resOpt->title == "A");

    std::filesystem::remove(file);
}

// ------------------------------------------------------------
// Test group: refreshFileHash
// ------------------------------------------------------------
TEST_CASE("FileService::refreshFileHash updates existing resource", "[FileService]") {
    SQLiteDB db(":memory:");
    createMinimalFileServiceSchema(db.get());

    FileRepository fileRepo(db);
    ResourceRepository resRepo(db);
    FileTextContentRepository fileTextRepo(db);
    FileService service(fileRepo, resRepo, fileTextRepo);

    // Tạo resource ID = 1
    sqlite3_exec(db.get(),
                 "INSERT INTO resources (id, title, type, file_hash) VALUES (1,'Old','pdf','123');",
                 nullptr, nullptr, nullptr);

    // Tạo file thật và insert dòng vào bảng files để mô phỏng file đã lưu
    auto file = createTempFile("refresh.txt", "new content");
    std::string const insertFileSql =
        "INSERT INTO files (resource_id, original_path, stored_path, is_managed) VALUES (1, '" +
        file.string() + "', '" + file.string() + "', 0);";
    REQUIRE(sqlite3_exec(db.get(), insertFileSql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK);

    // Gọi hàm cần test
    service.refreshFileHash(1);

    // Kiểm tra hash mới được cập nhật
    sqlite3_stmt* stmt = nullptr;
    REQUIRE(sqlite3_prepare_v2(db.get(), "SELECT file_hash FROM resources WHERE id=1;", -1, &stmt,
                               nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(stmt) == SQLITE_ROW);
    auto const* hash = reinterpret_cast<char const*>(sqlite3_column_text(stmt, 0));
    CHECK_FALSE(std::string(hash).empty());
    sqlite3_finalize(stmt);

    std::filesystem::remove(file);
}
