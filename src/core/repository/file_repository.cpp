#include <string>
#include <string_view>
#include <sqlite3.h>
#include <optional>
#include <vector>
#include <utility>

#include "model.hpp"
#include "sqldb_raii.hpp"
#include "file_repository.hpp"

namespace {
    inline std::string toUtf8String(const std::filesystem::path &p) {
        const std::u8string u8 = p.u8string();
        return {u8.begin(), u8.end()};
    }
} // namespace

void FileRepository::insertFile(sqlite3_int64 resourceId, const std::filesystem::path &storedPath,
                                const std::filesystem::path &originalPath, bool isManaged) {
    SQLiteStmt stmt(m_db.get(), "INSERT INTO files(resource_id, stored_path, original_path, "
                                "is_managed) VALUES (?, ?, ?, ?);");

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    const std::string storedUtf8 = toUtf8String(storedPath);
    const std::string originalUtf8 = toUtf8String(originalPath);

    if (isManaged) {
        // Liên kết nội bộ, sao chép file gốc vào thư mục lưu trữ nội bộ,
        // tồn tại 2 đường dẫn khác nhau, khi sử dụng: ưu tiên storedPath
        sqlite3_bind_text(stmt.get(), 2, storedUtf8.data(), static_cast<int>(storedUtf8.size()),
                          SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 3, originalUtf8.data(), static_cast<int>(originalUtf8.size()),
                          SQLITE_TRANSIENT);
    } else {
        // Liên kết ngoài, 2 đường dẫn giống nhau, khi sử dụng: ưu tiên storedPath
        sqlite3_bind_text(stmt.get(), 2, originalUtf8.data(), static_cast<int>(originalUtf8.size()),
                          SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 3, originalUtf8.data(), static_cast<int>(originalUtf8.size()),
                          SQLITE_TRANSIENT);
    }

    sqlite3_bind_int(stmt.get(), 4, static_cast<int>(isManaged));

    const int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Insert file info failed for resource ID: " +
                                 std::to_string(resourceId) + " Error: " + erroMSG);
    }
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
        sqlite3_bind_text(stmt.get(), 1, storedUtf8.data(), static_cast<int>(storedUtf8.size()),
                          SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, originalUtf8.data(), static_cast<int>(originalUtf8.size()),
                          SQLITE_TRANSIENT);
    } else {
        // Liên kết ngoài, 2 đường dẫn giống nhau, khi sử dụng: ưu tiên storedPath
        sqlite3_bind_text(stmt.get(), 1, originalUtf8.data(), static_cast<int>(originalUtf8.size()),
                          SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt.get(), 2, originalUtf8.data(), static_cast<int>(originalUtf8.size()),
                          SQLITE_TRANSIENT);
    }

    sqlite3_bind_int(stmt.get(), 3, static_cast<int>(isManaged));

    sqlite3_bind_int64(stmt.get(), 4, resourceId);

    const int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_CONSTRAINT) {
        throw std::runtime_error("Update failed: file path already exists -> " + storedUtf8);
    }

    if (rc != SQLITE_DONE) {
        std::string errMsg = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Update failed: " + errMsg);
    }

    if (sqlite3_changes(m_db.get()) == 0) {
        throw std::runtime_error("Update failed: no rows updated for resource ID: " +
                                 std::to_string(resourceId));
    }
}

std::optional<FileEntry> FileRepository::getFileById(sqlite_int64 resourceId) {
    SQLiteStmt stmt(m_db.get(), "SELECT resource_id, stored_path, original_path, is_managed FROM "
                                "files WHERE resource_id = ?;");

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        FileEntry entry;

        entry.resource_id = sqlite3_column_int64(stmt.get(), 0);

        if (sqlite3_column_type(stmt.get(), 1) != SQLITE_NULL) {
            const char* ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
            entry.stored_path = std::string(
                ptr, static_cast<std::string::size_type>(sqlite3_column_bytes(stmt.get(), 1)));
        }

        {
            const char* ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
            entry.original_path = std::string(
                ptr, static_cast<std::string::size_type>(sqlite3_column_bytes(stmt.get(), 2)));
        }

        entry.is_managed = sqlite3_column_int(stmt.get(), 3) != 0;

        return entry;
    }

    return std::nullopt;
}

bool FileRepository::exists(sqlite3_int64 resourceId) const {
    SQLiteStmt stmt(m_db.get(), "SELECT 1 FROM files WHERE resource_id = ? LIMIT 1;");

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    const int rc = sqlite3_step(stmt.get());

    return rc == SQLITE_ROW;
}

std::vector<FileEntry> FileRepository::getAllFile() {
    SQLiteStmt stmt(m_db.get(),
                    "SELECT resource_id, stored_path, original_path, is_managed FROM files;");

    std::vector<FileEntry> result;

    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        FileEntry entry;

        entry.resource_id = sqlite3_column_int64(stmt.get(), 0);

        if (sqlite3_column_type(stmt.get(), 1) != SQLITE_NULL) {
            const char* ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
            entry.stored_path = std::string(
                ptr, static_cast<std::string::size_type>(sqlite3_column_bytes(stmt.get(), 1)));
        }

        {
            const char* ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
            entry.original_path = std::string(
                ptr, static_cast<std::string::size_type>(sqlite3_column_bytes(stmt.get(), 2)));
        }

        entry.is_managed = sqlite3_column_int(stmt.get(), 3) != 0;

        result.push_back(std::move(entry));
    }

    return result;
}

std::optional<sqlite3_int64> FileRepository::getResourceIdBystoredPath(std::string_view path) {
    SQLiteStmt stmt(m_db.get(), "SELECT resource_id FROM files WHERE stored_path = ?;");

    sqlite3_bind_text(stmt.get(), 1, path.data(), static_cast<int>(path.size()), SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) { return sqlite3_column_int64(stmt.get(), 0); }

    return std::nullopt;
}

std::optional<sqlite3_int64>
    FileRepository::getResourceIdByOriginalPath(const std::filesystem::path &path) {
    SQLiteStmt stmt(m_db.get(), "SELECT resource_id FROM files WHERE original_path = ?;");

    const std::string pathUtf8 = toUtf8String(path);

    sqlite3_bind_text(stmt.get(), 1, pathUtf8.data(), static_cast<int>(pathUtf8.size()),
                      SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) { return sqlite3_column_int64(stmt.get(), 0); }

    return std::nullopt;
}

std::optional<std::string> FileRepository::getFilepathByResourceId(sqlite3_int64 resourceId) {
    SQLiteStmt stmt(m_db.get(), "SELECT stored_path FROM files WHERE resource_id = ?;");

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW &&
        sqlite3_column_type(stmt.get(), 0) != SQLITE_NULL) {
        const char* ptr = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        return std::string(
            ptr, static_cast<std::string::size_type>(sqlite3_column_bytes(stmt.get(), 0)));
    }

    return std::nullopt;
}

std::optional<sqlite3_int64> FileRepository::getResourceIdByFilepath(std::string_view filepath) {
    SQLiteStmt stmt(m_db.get(), "SELECT resource_id FROM files WHERE stored_path = ?;");

    sqlite3_bind_text(stmt.get(), 1, filepath.data(), static_cast<int>(filepath.size()),
                      SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) { return sqlite3_column_int64(stmt.get(), 0); }

    return std::nullopt;
}
