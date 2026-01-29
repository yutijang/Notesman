#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <sqlite3.h>
#include <vector>

#include "model.hpp"
#include "sqldb_raii.hpp"

class UrlRepository {
    public:
        explicit UrlRepository(SQLiteDB &db) noexcept : m_db(db) {}

        void insertUrl(sqlite3_int64 resourceId, std::string_view url,
                       std::string_view normalizedUrl, std::string_view domain);
        void updateUrl(sqlite3_int64 resourceId, std::string_view url,
                       std::string_view normalizedUrl, std::string_view domain);

        [[nodiscard]]
        std::optional<UrlEntry> getUrlByResourceId(sqlite3_int64 resourceId) const;

        [[nodiscard]] std::vector<UrlEntry> getAllUrls() const;

        [[nodiscard]]
        std::optional<sqlite3_int64>
            getResourceIdByNormalizedUrl(std::string_view normalizedUrl) const;

        [[nodiscard]]
        std::vector<sqlite3_int64> getResourceIdsByDomain(std::string_view domain) const;

        [[nodiscard]]
        std::optional<std::string> getUrlByResourceIdOnly(sqlite3_int64 resourceId) const;

        [[nodiscard]]
        std::optional<std::string> getDomainByResourceId(sqlite3_int64 resourceId) const;

        [[nodiscard]] bool exists(sqlite3_int64 resourceId) const;

    private:
        static UrlEntry urlEntryFromStmt(const SQLiteStmt &stmt);

        SQLiteDB &m_db;
};
