#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <utility>
#include <sqlite3.h>
#include "sqldb_raii.hpp"
#include "tag_repository.hpp"
#include "model.hpp"

std::optional<sqlite3_int64> TagRepository::addTag(std::string_view name) {
    SQLiteStmt stmt(m_db.get(), "INSERT INTO tags (name) VALUES (?) ON CONFLICT(name) DO NOTHING;");

    sqlite3_bind_text(stmt.get(), 1, name.data(), static_cast<int>(name.size()), SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Insert tag failed, reason: " + erroMSG);
    }

    if (sqlite3_changes(m_db.get()) == 0) { return getTagIdByName(name); }

    return sqlite3_last_insert_rowid(m_db.get());
}

std::vector<sqlite3_int64> TagRepository::addTags(const std::vector<std::string> &names) {
    if (names.empty()) { return {}; }

    // Mở transaction để tăng hiệu năng
    SQLiteStmt beginStmt(m_db.get(), "BEGIN TRANSACTION;");
    sqlite3_step(beginStmt.get());

    std::vector<sqlite3_int64> tagIds;
    tagIds.reserve(names.size());

    try {
        SQLiteStmt insertStmt(m_db.get(),
                              "INSERT INTO tags (name) VALUES (?) ON CONFLICT(name) DO NOTHING;");

        for (const auto &name : names) {
            sqlite3_reset(insertStmt.get());
            sqlite3_clear_bindings(insertStmt.get());
            sqlite3_bind_text(insertStmt.get(), 1, name.c_str(), static_cast<int>(name.size()),
                              SQLITE_TRANSIENT);

            const int rc = sqlite3_step(insertStmt.get());
            if (rc != SQLITE_DONE) {
                throw std::runtime_error("Insert tag failed: " +
                                         std::string(sqlite3_errmsg(m_db.get())));
            }

            sqlite3_int64 tagId{};
            if (sqlite3_changes(m_db.get()) == 0) {
                // Tag đã tồn tại, lấy ID cũ
                auto existingId = getTagIdByName(name);
                if (!existingId.has_value()) {
                    throw std::runtime_error("Tag exists but ID not found: " + name);
                }
                tagId = *existingId;
            } else {
                tagId = sqlite3_last_insert_rowid(m_db.get());
            }

            tagIds.push_back(tagId);
        }

        SQLiteStmt commitStmt(m_db.get(), "COMMIT;");
        sqlite3_step(commitStmt.get());

    } catch (...) {
        SQLiteStmt rollbackStmt(m_db.get(), "ROLLBACK;");
        sqlite3_step(rollbackStmt.get());
        throw;
    }

    return tagIds;
}

std::optional<sqlite3_int64> TagRepository::getTagIdByName(std::string_view name) {
    SQLiteStmt stmt(m_db.get(), "SELECT id FROM tags WHERE name = ?;");

    sqlite3_bind_text(stmt.get(), 1, name.data(), static_cast<int>(name.size()), SQLITE_TRANSIENT);

    const int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) { return sqlite3_column_int64(stmt.get(), 0); }
    if (rc == SQLITE_DONE) { return std::nullopt; }

    throw std::runtime_error("getTagIdByName failed, reason: " +
                             std::string(sqlite3_errmsg(m_db.get())));
}

void TagRepository::linkResourceIdWithTag(const ParamIDs &param) {
    SQLiteStmt stmt(m_db.get(), "INSERT INTO resource_tags (resource_id, tag_id) VALUES (?, ?);");

    sqlite3_bind_int64(stmt.get(), 1, param.resourceId);
    sqlite3_bind_int64(stmt.get(), 2, param.tagId);

    const int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Link ressource ID and tag ID failed, reason: " + erroMSG);
    }
}

void TagRepository::linkResourceWithTags(sqlite3_int64 resourceId,
                                         const std::vector<std::string> &tagNames) {
    auto tagIds = addTags(tagNames);

    SQLiteStmt beginStmt(m_db.get(), "BEGIN TRANSACTION;");
    sqlite3_step(beginStmt.get());

    try {
        SQLiteStmt stmt(m_db.get(),
                        "INSERT OR IGNORE INTO resource_tags (resource_id, tag_id) VALUES (?, ?);");

        for (auto tagId : tagIds) {
            sqlite3_reset(stmt.get());
            sqlite3_bind_int64(stmt.get(), 1, resourceId);
            sqlite3_bind_int64(stmt.get(), 2, tagId);

            const int rc = sqlite3_step(stmt.get());
            if (rc != SQLITE_DONE) {
                throw std::runtime_error("Link resource-tag failed: " +
                                         std::string(sqlite3_errmsg(m_db.get())));
            }
        }

        SQLiteStmt commitStmt(m_db.get(), "COMMIT;");
        sqlite3_step(commitStmt.get());

    } catch (...) {
        SQLiteStmt rollbackStmt(m_db.get(), "ROLLBACK;");
        sqlite3_step(rollbackStmt.get());
        throw;
    }
}

std::vector<std::pair<sqlite3_int64, std::string>>
    TagRepository::getTagsByResourceId(sqlite3_int64 resourceId) {
    SQLiteStmt stmt(m_db.get(), "SELECT t.id, t.name FROM tags t JOIN resource_tags rt ON t.id = "
                                "rt.tag_id WHERE rt.resource_id = ?;");

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    std::vector<std::pair<sqlite3_int64, std::string>> result;
    int rc{};
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        sqlite3_int64 id = sqlite3_column_int64(stmt.get(), 0);

        std::string name;
        if (sqlite3_column_type(stmt.get(), 1) != SQLITE_NULL) {
            name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        }
        result.emplace_back(id, std::move(name));
    }

    if (rc != SQLITE_DONE) {
        std::string erroMSG = sqlite3_errmsg(m_db.get());
        throw std::runtime_error("Has problem at getTagsByResourceId, reason: " + erroMSG);
    }

    return result;
}

std::vector<std::pair<sqlite3_int64, std::string>> TagRepository::getAllTags() {
    SQLiteStmt stmt(m_db.get(), "SELECT id, name FROM tags;");

    std::vector<std::pair<sqlite3_int64, std::string>> results;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        sqlite3_int64 id = sqlite3_column_int64(stmt.get(), 0);
        std::string name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        results.emplace_back(id, std::move(name));
    }

    return results;
}

std::vector<Resource> TagRepository::getResourcesViaTags(const std::vector<std::string> &tags) {
    if (tags.empty()) { return {}; }

    std::string sql = "SELECT r.id, r.title, r.type, r.created_at, r.updated_at "
                      "FROM resources r ";

    // JOIN nhiều lần để đảm bảo AND
    for (size_t i = 0; i < tags.size(); ++i) {
        sql += "JOIN resource_tags rt" + std::to_string(i) + " ON r.id = rt" + std::to_string(i) +
               ".resource_id "
               "JOIN tags t" +
               std::to_string(i) + " ON t" + std::to_string(i) + ".id = rt" + std::to_string(i) +
               ".tag_id ";
    }

    sql += "WHERE ";
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) { sql += " AND "; }
        sql += "t" + std::to_string(i) + ".name = ?";
    }

    SQLiteStmt stmt(m_db.get(), sql);

    for (size_t i = 0; i < tags.size(); ++i) {
        sqlite3_bind_text(stmt.get(), static_cast<int>(i + 1), tags[i].data(),
                          static_cast<int>(tags[i].size()), SQLITE_TRANSIENT);
    }

    std::vector<Resource> results;
    while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        Resource res{};
        res.id = sqlite3_column_int64(stmt.get(), 0);
        res.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        res.type = resourceTypeFromString(
            reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2)));
        res.created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
        res.updated_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));
        results.emplace_back(std::move(res));
    }

    return results;
}

std::vector<Resource> TagRepository::getResourcesViaOneTag(std::string_view name) {
    SQLiteStmt stmt(m_db.get(),
                    "SELECT r.id, r.title, r.type FROM resources r JOIN resource_tags rt ON r.id = "
                    "rt.resource_id JOIN tags t ON t.id = rt. tag_id WHERE t.name = ?;");

    sqlite3_bind_text(stmt.get(), 1, name.data(), static_cast<int>(name.size()), SQLITE_TRANSIENT);

    std::vector<Resource> results;
    int rc{};
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        Resource res{};

        res.id = sqlite3_column_int64(stmt.get(), 0);

        if (sqlite3_column_type(stmt.get(), 1) != SQLITE_NULL) {
            res.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
        }

        if (sqlite3_column_type(stmt.get(), 2) != SQLITE_NULL) {
            const char* typeText =
                reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
            res.type = resourceTypeFromString(typeText);
        } else {
            res.type = resourceTypeFromString("");
        }

        results.emplace_back(std::move(res));
    }

    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Has problem at getResourcesViaOneTag, reason: " +
                                 std::string(sqlite3_errmsg(m_db.get())));
    }

    return results;
}

void TagRepository::deleteTagFromResource(const ParamIDs &params) {
    SQLiteStmt stmt(m_db.get(), "DELETE FROM resource_tags WHERE resource_id = ? AND tag_id = ?;");

    sqlite3_bind_int64(stmt.get(), 1, params.resourceId);
    sqlite3_bind_int64(stmt.get(), 2, params.tagId);

    const int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Has problem at deleteTagFromResource, reason: " +
                                 std::string(sqlite3_errmsg(m_db.get())));
    }
}

void TagRepository::deleteAllTagsFromResource(sqlite3_int64 resourceId) {
    SQLiteStmt stmt(m_db.get(), "DELETE FROM resource_tags WHERE resource_id = ?;");

    sqlite3_bind_int64(stmt.get(), 1, resourceId);

    const int rc = sqlite3_step(stmt.get());
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Has problem at deleteAllTagsFromResource, reason: " +
                                 std::string(sqlite3_errmsg(m_db.get())));
    }
}
