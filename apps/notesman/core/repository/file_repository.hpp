#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <sqlite3.h>

#include "model.hpp"
#include "sqldb_raii.hpp"

class FileRepository {
    public:
        explicit FileRepository(SQLiteDB &db) noexcept : m_db(db) {}

        void insertFile(sqlite3_int64 resourceId, const std::filesystem::path &storedPath,
                        const std::filesystem::path &originalPath, bool isManaged);

        void updateFile(sqlite3_int64 resourceId, const std::filesystem::path &storedPath,
                        const std::filesystem::path &originalPath, bool isManaged);

        std::optional<FileEntry> getFileById(sqlite_int64 resourceId);
        std::vector<FileEntry> getAllFile();

        std::optional<sqlite3_int64> getResourceIdBystoredPath(std::string_view path);
        std::optional<sqlite3_int64> getResourceIdByOriginalPath(const std::filesystem::path &path);

        std::optional<std::string> getFilepathByResourceId(sqlite3_int64 resourceId);
        std::optional<sqlite3_int64> getResourceIdByFilepath(std::string_view filepath);

        [[nodiscard]] bool exists(sqlite3_int64 resourceId) const;

    private:
        SQLiteDB &m_db;
};
