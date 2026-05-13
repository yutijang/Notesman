#pragma once

#include "model.hpp"
#include "sqldb_raii.hpp"

#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <vector>

class UrlRepository {
  public:
    explicit UrlRepository(SQLiteDB& db) noexcept : m_db(db) {}

    void insertUrl(sqlite3_int64 resourceId,
                   std::string_view rawUrl,
                   std::string_view normalizedUrl,
                   std::string_view domain,
                   std::string_view urlPath);
    void updateUrl(sqlite3_int64 resourceId,
                   std::string_view rawUrl,
                   std::string_view normalizedUrl,
                   std::string_view domain,
                   std::string_view urlPath);

    [[nodiscard]]
    std::optional<UrlEntry> getUrlByResourceId(sqlite3_int64 resourceId) const;

    [[nodiscard]] std::vector<UrlEntry> getAllUrls() const;

    [[nodiscard]]
    std::optional<sqlite3_int64> getResourceIdByNormalizedUrl(std::string_view normalizedUrl) const;

    // Lấy toàn bộ resource_id có URL thuộc cùng domain
    [[nodiscard]]
    std::vector<sqlite3_int64> getResourceIdsByDomain(std::string_view domain) const;

    // Với một resource_id đã biết, nếu resource đó là URL thì lấy raw URL người dùng đã nhập.
    [[nodiscard]]
    std::optional<std::string> getUrlByResourceIdOnly(sqlite3_int64 resourceId) const;

    // Với một resource_id, nếu resource đó là URL thì lấy domain của URL
    [[nodiscard]]
    std::optional<std::string> getDomainByResourceId(sqlite3_int64 resourceId) const;

    // Kiểm tra resource_id này có entry URL hay không
    [[nodiscard]] bool exists(sqlite3_int64 resourceId) const;

  private:
    static UrlEntry urlEntryFromStmt(SQLiteStmt const& stmt);

    SQLiteDB& m_db;
};
