#include "tag_repository.hpp"

#include "model.hpp"
#include "sqldb_raii.hpp"
#include "sqlite_utils.hpp"

#include <cstddef>
#include <optional>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

std::optional<sqlite3_int64> TagRepository::addTag(std::string_view name) {
    SQLiteStmt stmt(m_db.get(), "INSERT INTO tags (name) VALUES (?) ON CONFLICT(name) DO NOTHING;");

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, name.data(), static_cast<int>(name.size()),
                                        SQLITE_TRANSIENT),
                      m_db.get());

    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE, "Insert tag");

    if (sqlite3_changes(m_db.get()) == 0) { return getTagIdByName(name); }

    return sqlite3_last_insert_rowid(m_db.get());
}

std::vector<sqlite3_int64> TagRepository::addTags(std::vector<std::string> const& names) {
    if (names.empty()) { return {}; }

    // Mở transaction để tăng hiệu năng
    SQLiteStmt beginStmt(m_db.get(), "BEGIN TRANSACTION;");
    sqlite3_step(beginStmt.get());

    std::vector<sqlite3_int64> tagIds;
    tagIds.reserve(names.size());

    try {
        SQLiteStmt insertStmt(m_db.get(),
                              "INSERT INTO tags (name) VALUES (?) ON CONFLICT(name) DO NOTHING;");

        for (auto const& name : names) {
            insertStmt.reset();
            insertStmt.clearBindings();

            sqlite::checkBind(sqlite3_bind_text(insertStmt.get(), 1, name.c_str(),
                                                static_cast<int>(name.size()), SQLITE_TRANSIENT),
                              m_db.get());

            sqlite::checkStep(insertStmt.step(), m_db.get(), SQLITE_DONE, "Insert tag");

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

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, name.data(), static_cast<int>(name.size()),
                                        SQLITE_TRANSIENT),
                      m_db.get());

    int const rc = stmt.step();

    if (rc == SQLITE_ROW) { return stmt.getColumnInt64(0); }

    if (rc == SQLITE_DONE) { return std::nullopt; }

    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        sqlite::checkStep(rc, m_db.get(), SQLITE_ROW, "getTagIdByName (unexpected rc)");
    }

    return std::nullopt;
}

void TagRepository::linkResourceIdWithTag(ParamIDs const& param) {
    SQLiteStmt stmt(m_db.get(), "INSERT INTO resource_tags (resource_id, tag_id) VALUES (?, ?);");

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, param.resourceId), m_db.get());
    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 2, param.tagId), m_db.get());

    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE, "Link ressource ID and tag ID");
}

void TagRepository::linkResourceWithTags(sqlite3_int64 resourceId,
                                         std::vector<std::string> const& tagNames) {
    auto tagIds = addTags(tagNames);

    SQLiteStmt beginStmt(m_db.get(), "BEGIN TRANSACTION;");
    sqlite3_step(beginStmt.get());

    try {
        SQLiteStmt stmt(m_db.get(),
                        "INSERT OR IGNORE INTO resource_tags (resource_id, tag_id) VALUES (?, ?);");

        for (auto tagId : tagIds) {
            stmt.reset();

            sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());
            sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 2, tagId), m_db.get());

            sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE, "Link resource-tag");
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

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());

    std::vector<std::pair<sqlite3_int64, std::string>> result;
    int rc{};
    while ((rc = stmt.step()) == SQLITE_ROW) {
        sqlite3_int64 id = stmt.getColumnInt64(0);
        auto name = stmt.getColumnText(1);
        result.emplace_back(id, std::move(name));
    }

    sqlite::checkStep(rc, m_db.get(), SQLITE_DONE, "getTagsByResourceId");

    return result;
}

std::vector<std::pair<sqlite3_int64, std::string>> TagRepository::getAllTags() {
    SQLiteStmt stmt(m_db.get(), "SELECT id, name FROM tags;");

    std::vector<std::pair<sqlite3_int64, std::string>> results;
    while (stmt.step() == SQLITE_ROW) {
        sqlite3_int64 id = stmt.getColumnInt64(0);
        auto name = stmt.getColumnText(1);
        results.emplace_back(id, std::move(name));
    }

    return results;
}

std::vector<Resource> TagRepository::getResourcesViaTags(std::vector<std::string> const& tags) {
    if (tags.empty()) { return {}; }

    std::string sql = "SELECT r.id, r.title, r.type, r.created_at, r.updated_at "
                      "FROM resources r ";

    // JOIN nhiều lần để đảm bảo AND
    for (std::size_t i = 0; i < tags.size(); ++i) {
        sql += "JOIN resource_tags rt" + std::to_string(i) + " ON r.id = rt" + std::to_string(i) +
               ".resource_id "
               "JOIN tags t" +
               std::to_string(i) + " ON t" + std::to_string(i) + ".id = rt" + std::to_string(i) +
               ".tag_id ";
    }

    sql += "WHERE ";
    for (std::size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) { sql += " AND "; }
        sql += "t" + std::to_string(i) + ".name = ?";
    }

    SQLiteStmt stmt(m_db.get(), sql);

    for (std::size_t i = 0; i < tags.size(); ++i) {
        sqlite::checkBind(sqlite3_bind_text(stmt.get(), static_cast<int>(i + 1), tags[i].data(),
                                            static_cast<int>(tags[i].size()), SQLITE_TRANSIENT),
                          m_db.get());
    }

    std::vector<Resource> results;
    while (stmt.step() == SQLITE_ROW) {
        Resource res{};
        res.id = stmt.getColumnInt64(0);
        res.title = stmt.getColumnText(1);
        res.type = resourceTypeFromString(stmt.getColumnText(2));
        res.created_at = stmt.getColumnText(3);
        res.updated_at = stmt.getColumnText(4);
        results.emplace_back(std::move(res));
    }

    return results;
}

std::vector<Resource> TagRepository::getResourcesViaOneTag(std::string_view name) {
    SQLiteStmt stmt(m_db.get(),
                    "SELECT r.id, r.title, r.type FROM resources r JOIN resource_tags rt ON r.id = "
                    "rt.resource_id JOIN tags t ON t.id = rt. tag_id WHERE t.name = ?;");

    sqlite::checkBind(sqlite3_bind_text(stmt.get(), 1, name.data(), static_cast<int>(name.size()),
                                        SQLITE_TRANSIENT),
                      m_db.get());

    std::vector<Resource> results;
    int rc{};
    while ((rc = stmt.step()) == SQLITE_ROW) {
        Resource res{};

        res.id = stmt.getColumnInt64(0);

        res.title = stmt.getColumnText(1);

        if (sqlite3_column_type(stmt.get(), 2) != SQLITE_NULL) {
            res.type = resourceTypeFromString(stmt.getColumnText(2));
        } else {
            res.type = resourceTypeFromString("");
        }

        results.emplace_back(std::move(res));
    }

    sqlite::checkStep(rc, m_db.get(), SQLITE_DONE, "getResourcesViaOneTag");

    return results;
}

void TagRepository::deleteTagFromResource(ParamIDs const& params) {
    SQLiteStmt stmt(m_db.get(), "DELETE FROM resource_tags WHERE resource_id = ? AND tag_id = ?;");

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, params.resourceId), m_db.get());
    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 2, params.tagId), m_db.get());

    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE, "deleteTagFromResource");
}

void TagRepository::deleteAllTagsFromResource(sqlite3_int64 resourceId) {
    SQLiteStmt stmt(m_db.get(), "DELETE FROM resource_tags WHERE resource_id = ?;");

    sqlite::checkBind(sqlite3_bind_int64(stmt.get(), 1, resourceId), m_db.get());
    sqlite::checkStep(stmt.step(), m_db.get(), SQLITE_DONE, "deleteAllTagsFromResource");
}
