#pragma once

#include "model.hpp"
#include "sqldb_raii.hpp"

#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class TagRepository {
  public:
    struct ParamIDs {
        sqlite3_int64 resourceId;
        sqlite3_int64 tagId;
    };

    explicit TagRepository(SQLiteDB& db) noexcept : m_db(db) {}

    // Return tag_id
    std::optional<sqlite3_int64> addTag(std::string_view name);
    std::vector<sqlite3_int64> addTags(std::vector<std::string> const& names);

    std::optional<sqlite3_int64> getTagIdByName(std::string_view name);
    void linkResourceIdWithTag(ParamIDs const& params);
    void linkResourceWithTags(sqlite3_int64 resourceId, std::vector<std::string> const& tagNames);
    std::vector<std::pair<sqlite3_int64, std::string>>
        getTagsByResourceId(sqlite3_int64 resourceId);
    std::vector<std::pair<sqlite3_int64, std::string>> getAllTags();
    std::vector<Resource> getResourcesViaTags(std::vector<std::string> const& tags);
    std::vector<Resource> getResourcesViaOneTag(std::string_view name);

    void deleteTagFromResource(ParamIDs const& params);
    void deleteAllTagsFromResource(sqlite3_int64 resourceId);

  private:
    SQLiteDB& m_db;
};
