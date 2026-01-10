#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <utility>
#include <sqlite3.h>

#include "model.hpp"
#include "sqldb_raii.hpp"
#include "file_repository.hpp"
#include "sqlite_utils.hpp"

namespace {
    inline std::string toUtf8String(const std::filesystem::path &p) {
        const std::u8string u8 = p.u8string();
        return {u8.begin(), u8.end()};
    }
} // namespace

void FileRepository::insertFile(sqlite3_int64 resourceId, const std::filesystem::path &storedPath,
                                const std::filesystem::path &originalPath, bool isManaged) {
    static constexpr const char* sql = "INSERT INTO files(resource_id, stored_path, original_path, "
                                       "is_managed) VALUES (?, ?, ?, ?);";
    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    const std::string storedUtf8 = toUtf8String(storedPath);
    const std::string originalUtf8 = toUtf8String(originalPath);

    if (isManaged) {
        // Liên kết nội bộ, sao chép file gốc vào thư mục lưu trữ nội bộ,
        // tồn tại 2 đường dẫn khác nhau, khi sử dụng: ưu tiên storedPath
        sqlite::checkBind(sqlite3_bind_text(stmt.get(), 2, storedUtf8.data(),
                                            static_cast<int>(storedUtf8.size()), SQLITE_TRANSIENT),
                          m_db.get());
        sqlite::checkBind(sqlite3_bind_text(stmt.get(), 3, originalUtf8.data(),
                                            static_cast<int>(originalUtf8.size()),
                                            SQLITE_TRANSIENT),
                          m_db.get());
    } else {
        // Liên kết ngoài, 2 đường dẫn giống nhau, khi sử dụng: ưu tiên storedPath
        sqlite::checkBind(sqlite3_bind_text(stmt.get(), 2, originalUtf8.data(),
                                            static_cast<int>(originalUtf8.size()),
                                            SQLITE_TRANSIENT),
                          m_db.get());
        sqlite::checkBind(sqlite3_bind_text(stmt.get(), 3, originalUtf8.data(),
                                            static_cast<int>(originalUtf8.size()),
                                            SQLITE_TRANSIENT),
                          m_db.get());
    }

    sqlite::checkBind(sqlite3_bind_int(stmt.get(), 4, static_cast<int>(isManaged)), m_db.get());
    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE,
                      "insertFile - Resource ID: " + std::to_string(resourceId));
}

void FileRepository::updateFile(sqlite3_int64 resourceId, const std::filesystem::path &storedPath,
                                const std::filesystem::path &originalPath, bool isManaged) {
    SQLiteStmt stmt(m_db.get(), "UPDATE files SET stored_path = ?, original_path = ?, is_managed = "
                                "? WHERE resource_id = ?;");

    const std::string storedUtf8 = toUtf8String(storedPath);
    const std::string originalUtf8 = toUtf8String(originalPath);

    if (isManaged) {
        // Liên kết nội bộ, sao chép file gốc vào thư mục lưu trữ nội bộ,
        // tồn tại 2 đường dẫn khác nhau, khi sử dụng: ưu tiên storedPath
        sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, storedUtf8.data(),
                                            static_cast<int>(storedUtf8.size()), SQLITE_TRANSIENT),
                          m_db.get());
        sqlite::checkBind(sqlite3_bind_text(stmt.get(), 2, originalUtf8.data(),
                                            static_cast<int>(originalUtf8.size()),
                                            SQLITE_TRANSIENT),
                          m_db.get());
    } else {
        // Liên kết ngoài, 2 đường dẫn giống nhau, khi sử dụng: ưu tiên storedPath
        sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, originalUtf8.data(),
                                            static_cast<int>(originalUtf8.size()),
                                            SQLITE_TRANSIENT),
                          m_db.get());
        sqlite::checkBind(sqlite3_bind_text(stmt.get(), 2, originalUtf8.data(),
                                            static_cast<int>(originalUtf8.size()),
                                            SQLITE_TRANSIENT),
                          m_db.get());
    }

    sqlite::checkBind(sqlite3_bind_int(stmt.get(), 3, static_cast<int>(isManaged)), m_db.get());
    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 4, resourceId), m_db.get());

    const int rc = stmt.step();
    sqlite::checkStep(rc, m_db.get(), SQLITE_CONSTRAINT, "updateFile: " + storedUtf8);
    sqlite::checkStep(rc, m_db.get(), SQLITE_DONE, "updateFile");

    if (sqlite3_changes(m_db.get()) == 0) {
        throw std::runtime_error("Update failed: no rows updated for resource ID: " +
                                 std::to_string(resourceId));
    }
}

std::optional<FileEntry> FileRepository::getFileById(sqlite_int64 resourceId) {
    static constexpr const char* sql = "SELECT resource_id, stored_path, original_path, is_managed "
                                       "FROM files "
                                       "WHERE resource_id = ?;";
    SQLiteStmt stmt(m_db.get(), sql);

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    if (stmt.step() == SQLITE_ROW) {
        FileEntry entry;

        entry.resource_id = sqlite3_column_int64(stmt.get(), 0);

        if (sqlite3_column_type(stmt.get(), 1) != SQLITE_NULL) {
            entry.stored_path = stmt.getColumnText(1);
        }

        { entry.original_path = stmt.getColumnText(2); }

        entry.is_managed = sqlite3_column_int(stmt.get(), 3) != 0;

        return entry;
    }

    return std::nullopt;
}

bool FileRepository::exists(sqlite3_int64 resourceId) const {
    SQLiteStmt stmt(m_db.get(), "SELECT 1 FROM files WHERE resource_id = ? LIMIT 1;");

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    return stmt.step() == SQLITE_ROW;
}

std::vector<FileEntry> FileRepository::getAllFile() {
    static constexpr const char* sql = "SELECT resource_id, stored_path, original_path, is_managed "
                                       "FROM files;";
    SQLiteStmt stmt(m_db.get(), sql);

    std::vector<FileEntry> result;

    while (stmt.step() == SQLITE_ROW) {
        FileEntry entry;

        entry.resource_id = sqlite3_column_int64(stmt.get(), 0);

        if (sqlite3_column_type(stmt.get(), 1) != SQLITE_NULL) {
            entry.stored_path = stmt.getColumnText(1);
        }

        { entry.original_path = stmt.getColumnText(2); }

        entry.is_managed = sqlite3_column_int(stmt.get(), 3) != 0;

        result.push_back(std::move(entry));
    }

    return result;
}

std::optional<sqlite3_int64> FileRepository::getResourceIdBystoredPath(std::string_view path) {
    SQLiteStmt stmt(m_db.get(), "SELECT resource_id FROM files WHERE stored_path = ?;");

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, path.data(), static_cast<int>(path.size()),
                                        SQLITE_TRANSIENT),
                      m_db.get());

    if (stmt.step() == SQLITE_ROW) { return sqlite3_column_int64(stmt.get(), 0); }

    return std::nullopt;
}

std::optional<sqlite3_int64>
    FileRepository::getResourceIdByOriginalPath(const std::filesystem::path &path) {
    SQLiteStmt stmt(m_db.get(), "SELECT resource_id FROM files WHERE original_path = ?;");

    const std::string pathUtf8 = toUtf8String(path);

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, pathUtf8.data(),
                                        static_cast<int>(pathUtf8.size()), SQLITE_TRANSIENT),
                      m_db.get());

    if (stmt.step() == SQLITE_ROW) { return sqlite3_column_int64(stmt.get(), 0); }

    return std::nullopt;
}

std::optional<std::string> FileRepository::getFilepathByResourceId(sqlite3_int64 resourceId) {
    SQLiteStmt stmt(m_db.get(), "SELECT stored_path FROM files WHERE resource_id = ?;");

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    if (stmt.step() == SQLITE_ROW && sqlite3_column_type(stmt.get(), 0) != SQLITE_NULL) {
        return stmt.getColumnText(0);
    }

    return std::nullopt;
}

std::optional<sqlite3_int64> FileRepository::getResourceIdByFilepath(std::string_view filepath) {
    SQLiteStmt stmt(m_db.get(), "SELECT resource_id FROM files WHERE stored_path = ?;");

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, filepath.data(),
                                        static_cast<int>(filepath.size()), SQLITE_TRANSIENT),
                      m_db.get());
    if (stmt.step() == SQLITE_ROW) { return sqlite3_column_int64(stmt.get(), 0); }

    return std::nullopt;
}
