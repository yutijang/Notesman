#pragma once

#include "model.hpp"
#include "sqldb_raii.hpp"

#include <filesystem>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <vector>

class FileRepository {
    public:
        explicit FileRepository(SQLiteDB& db) noexcept : m_db(db) {}

        void insertFile(sqlite3_int64 resourceId, std::filesystem::path const& storedPath,
                        std::filesystem::path const& originalPath, bool isManaged);

        void updateFile(sqlite3_int64 resourceId, std::filesystem::path const& storedPath,
                        std::filesystem::path const& originalPath, bool isManaged);

        std::optional<FileEntry> getFileById(sqlite_int64 resourceId);
        std::vector<FileEntry> getAllFile();

        std::optional<sqlite3_int64> getResourceIdBystoredPath(std::string_view path);
        std::optional<sqlite3_int64> getResourceIdByOriginalPath(std::filesystem::path const& path);

        std::optional<std::string> getFilepathByResourceId(sqlite3_int64 resourceId);
        std::optional<sqlite3_int64> getResourceIdByFilepath(std::string_view filepath);

        // kiểm tra có bản ghi file gắn với resource này hay không
        // xác định 1 tài nguyên có phải là dạng file hay không
        [[nodiscard]] bool exists(sqlite3_int64 resourceId) const;

        std::optional<std::string> getResolvedPath(sqlite_int64 resourceId);

    private:
        SQLiteDB& m_db;
};
