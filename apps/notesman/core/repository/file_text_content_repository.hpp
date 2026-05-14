#pragma once

#include "core/db/sqldb_raii.hpp"

#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>

class FileTextContentRepository {
  public:
    explicit FileTextContentRepository(SQLiteDB& db) noexcept : m_db(db) {}

    void upsertText(sqlite3_int64 resourceId, std::string_view text);
    std::optional<std::string> getTextById(sqlite3_int64 resourceId);
    bool isIndexed(sqlite3_int64 resourceId);

  private:
    SQLiteDB& m_db;
};
