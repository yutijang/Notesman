#include <stdexcept>
#include <optional>
#include <sqlite3.h>
#include "resource_repository.hpp"
#include "model.hpp"
#include "sqldb_raii.hpp"

sqlite3_int64 ResourceRepository::insert(const Resource &res) {
    SQLiteStmt stmt(m_db.get(), "INSERT INTO resources (title, type, file_hash) VALUES (?, ?, ?);");

    sqlite3_bind_text(stmt.get(), 1, res.title.c_str(), -1, SQLITE_TRANSIENT);

    const char* typeStr = resourceTypeToString(res.type);
    sqlite3_bind_text(stmt.get(), 2, typeStr, -1, SQLITE_TRANSIENT);

    if (res.type == ResourceType::text || res.file_hash.empty()) {
        sqlite3_bind_null(stmt.get(), 3);
    } else {
        sqlite3_bind_text(stmt.get(), 3, res.file_hash.c_str(), -1, SQLITE_TRANSIENT);
    }

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Insert failed for resource: " + res.title + " Error: " + erroMSG);
    }

    return sqlite3_last_insert_rowid(m_db.get());
}

std::optional<Resource> ResourceRepository::getById(sqlite3_int64 resourceId) {
    SQLiteStmt stmt(m_db.get(),
                    "SELECT id, title, type, created_at, updated_at FROM resources WHERE id = ?;");

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        Resource res;

        res.id = sqlite3_column_int64(stmt.get(), 0);
        res.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));

        const char* typeText = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        res.type = resourceTypeFromString(typeText);

        res.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
        res.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));

        return res;
    }

    return std::nullopt;
}

std::vector<Resource> ResourceRepository::getAll() {
    SQLiteStmt stmt(m_db.get(), "SELECT id, title, type, created_at, updated_at FROM resources;");

    std::vector<Resource> results;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        Resource res;
        res.id = sqlite3_column_int64(stmt.get(), 0);
        res.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));

        const char* typeText = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        res.type = resourceTypeFromString(typeText);

        res.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
        res.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));
        results.push_back(std::move(res));
    }
    return results;
}

std::vector<Resource> ResourceRepository::searchByTitleFTS(std::string_view keyword) {
    SQLiteStmt stmt(m_db.get(),
                    "SELECT r.id, r.title, r.type, r.file_hash, r.created_at, r.updated_at FROM "
                    "resources r JOIN resources_fts ON r.id = resources_fts.rowid WHERE "
                    "resources_fts MATCH ?;");

    sqlite3_bind_text(stmt.get(), 1, keyword.data(), static_cast<int>(keyword.size()),
                      SQLITE_TRANSIENT);

    std::vector<Resource> result;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        Resource res;
        res.id = sqlite3_column_int64(stmt.get(), 0);
        res.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));

        const char* typeText = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        res.type = resourceTypeFromString(typeText);

        if (sqlite3_column_type(stmt.get(), 3) != SQLITE_NULL) {
            res.file_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
        }

        res.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));
        res.updated_at = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt.get(), 5)); // NOLINT(readability-magic-numbers)

        result.push_back(std::move(res));
    }

    return result;
}

std::optional<Resource> ResourceRepository::getByFileHash(std::string_view hash) {
    SQLiteStmt stmt(m_db.get(), "SELECT id, title, type, file_hash, created_at, updated_at FROM "
                                "resources WHERE file_hash = ?;");

    sqlite3_bind_text(stmt.get(), 1, hash.data(), static_cast<int>(hash.size()), SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        Resource res;
        res.id = sqlite3_column_int64(stmt.get(), 0);
        res.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));

        const char* typeText = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
        res.type = resourceTypeFromString(typeText);

        if (sqlite3_column_type(stmt.get(), 3) != SQLITE_NULL) {
            res.file_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
        }

        res.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));
        res.updated_at = reinterpret_cast<const char*>(
            sqlite3_column_text(stmt.get(), 5)); // NOLINT(readability-magic-numbers)

        return res;
    }

    return std::nullopt;
}

std::optional<std::pair<std::string, std::string>>
    ResourceRepository::getTimestamps(sqlite3_int64 resourceID) {
    SQLiteStmt stmt(m_db.get(), "SELECT created_at, updated_at FROM resources WHERE id = ?;");

    sqlite3_bind_int64(stmt.get(), 1, resourceID);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        std::string createdAt;
        std::string updatedAt;

        if (sqlite3_column_type(stmt.get(), 0) != SQLITE_NULL) {
            createdAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
        }

        if (sqlite3_column_type(stmt.get(), 1) != SQLITE_NULL) {
            updatedAt = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        }

        return std::make_pair(std::move(createdAt), std::move(updatedAt));
    }

    return std::nullopt;
}

void ResourceRepository::update(const Resource &res) {
    SQLiteStmt stmt(m_db.get(), "UPDATE resources SET title = ?, type = ?, updated_at = "
                                "CURRENT_TIMESTAMP WHERE id = ?;");

    sqlite3_bind_text(stmt.get(), 1, res.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, resourceTypeToString(res.type), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 3, res.id);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Update failed: " + erroMSG);
    }
}

void ResourceRepository::remove(sqlite3_int64 resourceId) {
    SQLiteStmt stmt(m_db.get(), "DELETE FROM resources WHERE id = ?;");

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Delete failed: " + erroMSG);
    }
}

void ResourceRepository::updateFileHash(sqlite3_int64 resourceID, std::string_view hash) {
    SQLiteStmt stmt(m_db.get(), "UPDATE resources SET file_hash = ? WHERE id = ?;");

    sqlite3_bind_text(stmt.get(), 1, hash.data(), static_cast<int>(hash.size()), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt.get(), 2, resourceID);

    if (sqlite3_step(stmt.get()) != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Update failed: " + erroMSG);
    }
}

bool ResourceRepository::existsTitle(std::string_view title, ResourceType type) const {
    SQLiteStmt stmt(
        m_db.get(),
        "SELECT EXISTS (SELECT 1 FROM resources WHERE title = ? AND type = ? LIMIT 1);");

    sqlite3_bind_text(stmt.get(), 1, title.data(), static_cast<int>(title.size()),
                      SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, resourceTypeToString(type), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt.get()) == SQLITE_ROW) { return sqlite3_column_int(stmt.get(), 0) != 0; }

    return false;
}
