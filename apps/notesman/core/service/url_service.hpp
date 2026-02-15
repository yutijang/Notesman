#pragma once

#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>

#include "model.hpp"
#include "resource_repository.hpp"
#include "url_repository.hpp"

class UrlService {
    public:
        UrlService(UrlRepository& urlRepo, ResourceRepository& resRepo) noexcept
            : m_urlRepo(urlRepo), m_resRepo(resRepo) {}

        // Core CURD
        std::optional<sqlite3_int64> addUrlResource(std::string_view title, ResourceType type,
                                                    std::string_view rawUrl);
        void updateUrl(sqlite3_int64 resourceId, std::string_view rawUrl);

        // Search
        [[nodiscard]]
        std::optional<std::string> getUrlByResourceIdOnly(sqlite3_int64 resourceId) const;

    private:
        struct UrlParts {
                std::string domain;
                std::string path;
        };

        // ========== Utils ==========
        static bool isValidUrl(std::string_view urlRaw);

        // Mục tiêu của normalizeUrl():
        // KHÔNG xoá query(vì query có thể là identity)
        // XOÁ fragment
        // Chuẩn hoá scheme/host/port
        // Chuẩn hoá path(./..)
        // Chuẩn hoá query: parse → sort → rebuild
        // Output dùng cho:
        // - normalized_url(UNIQUE, chống trùng logic)
        // - so sánh identity tài nguyên
        static std::optional<std::string> normalizeUrl(std::string_view rawUrl);

        static std::optional<std::string> extractDomain(std::string_view normalizedUrl);
        static std::optional<std::string> extractPath(std::string_view normalizedUrl);

        static std::optional<UrlService::UrlParts> getUrlParts(std::string_view normalizedUrl);

        UrlRepository& m_urlRepo;
        ResourceRepository& m_resRepo;
};
