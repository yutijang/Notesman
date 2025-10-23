#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <vector>
#include <utility>
#include <sqlite3.h>
#include "model.hpp"

class SQLiteDB;

class TagRepository {
    public:
        struct ParamIDs {
                sqlite3_int64 resourceId;
                sqlite3_int64 tagId;
        };

        explicit TagRepository(SQLiteDB &db) noexcept : m_db(db) {}

        // Return tag_id
        std::optional<sqlite3_int64> addTag(std::string_view name);
        std::vector<sqlite3_int64> addTags(const std::vector<std::string> &names);

        std::optional<sqlite3_int64> getTagIdByName(std::string_view name);
        void linkResourceIdWithTag(const ParamIDs &params);
        void linkResourceWithTags(sqlite3_int64 resourceId,
                                  const std::vector<std::string> &tagNames);
        std::vector<std::pair<sqlite3_int64, std::string>>
            getTagsByResourceId(sqlite3_int64 resourceId);
        std::vector<std::pair<sqlite3_int64, std::string>> getAllTags();
        std::vector<Resource> getResourcesViaTags(const std::vector<std::string> &tags);
        std::vector<Resource> getResourcesViaOneTag(std::string_view name);

        void deleteTagFromResource(const ParamIDs &params);
        void deleteAllTagsFromResource(sqlite3_int64 resourceId);

    private:
        SQLiteDB &m_db;
};
