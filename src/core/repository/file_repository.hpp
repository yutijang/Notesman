#pragma once

#include <string_view>
#include <sqlite3.h>
#include <optional>
#include <vector>
#include "model.hpp"

class SQLiteDB;

class FileRepository {
    public:
        explicit FileRepository(SQLiteDB &db) noexcept : m_db(db) {}

        void insertFile(sqlite3_int64 resourceId, std::string_view storedPath,
                        std::string_view originalPath, bool isManaged);

        void updateFile(sqlite3_int64 resourceId, std::string_view storedPath,
                        std::string_view originalPath, bool isManaged);

        std::optional<FileEntry> getFileById(sqlite_int64 resourceId);
        std::vector<FileEntry> getAllFile();

        std::optional<sqlite3_int64> getResourceIdBystoredPath(std::string_view path);
        std::optional<sqlite3_int64> getResourceIdByOriginalPath(std::string_view path);

        [[nodiscard]] bool exists(sqlite3_int64 resourceId) const;

    private:
        SQLiteDB &m_db;
};
