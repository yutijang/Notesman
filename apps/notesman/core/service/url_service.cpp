#include "core/service/url_service.hpp"

#include "common/logger/Logger.hpp"
#include "core/model/model.hpp"

#include <algorithm>
#include <boost/url/param.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace urls = boost::urls;

std::optional<sqlite3_int64>
    UrlService::addUrlResource(std::string_view title, ResourceType type, std::string_view rawUrl) {
    auto normalizedUrl = normalizeUrl(rawUrl);
    if (!normalizedUrl) {
        return std::nullopt;
    }

    if (auto resIdOpt = m_urlRepo.getResourceIdByNormalizedUrl(*normalizedUrl)) {
        return resIdOpt;
    }

    // NOLINTNEXTLINE (-Wmissing-designated-field-initializers)
    sqlite3_int64 resourceId = m_resRepo.insert({.title = std::string(title), .type = type});
    auto partsOpt = getUrlParts(*normalizedUrl);
    if (!partsOpt) {
        return std::nullopt;
    }

    m_urlRepo.insertUrl(resourceId, rawUrl, *normalizedUrl, partsOpt->domain, partsOpt->path);

    return resourceId;
}

void UrlService::updateUrl(sqlite3_int64 resourceId, std::string_view rawUrl) {
    auto normalizedUrl = normalizeUrl(rawUrl);
    if (!normalizedUrl) [[unlikely]] {
        Log::err("Invalid URL");
        return;
    }

    auto partsOpt = getUrlParts(*normalizedUrl);
    if (!partsOpt) [[unlikely]] {
        Log::err("Error get parts");
        return;
    }

    m_urlRepo.updateUrl(resourceId, rawUrl, *normalizedUrl, partsOpt->domain, partsOpt->path);
}

std::optional<std::string> UrlService::getUrlByResourceIdOnly(sqlite3_int64 resourceId) const {
    return m_urlRepo.getUrlByResourceIdOnly(resourceId);
}

// ========= Utils =========
bool UrlService::isValidUrl(std::string_view urlRaw) {
    return urls::parse_absolute_uri(urlRaw).has_value();
}

std::optional<std::string> UrlService::normalizeUrl(std::string_view rawUrl) {
    auto parsed = urls::parse_uri_reference(rawUrl);
    if (!parsed) [[unlikely]] {
        Log::err("Invalid URL");
        return std::nullopt;
    }

    urls::url u = *parsed;

    // scheme + host already canonicalized by Boost.URL

    // default port
    std::string_view const scheme = u.scheme();
    if ((scheme == "http" && u.port() == "80") || (scheme == "https" && u.port() == "443")) {
        u.remove_port();
    }

    // path
    u.normalize_path();

    // fragment: NOT identity
    u.remove_fragment();

    // query: identity → canonicalize
    if (u.has_query()) {
        auto view = u.params();

        std::vector<std::pair<std::string, std::string>> params;
        params.reserve(view.size());

        for (auto const& p : view) {
            params.emplace_back(p.key, p.value);
        }

        std::ranges::sort(params);

        u.remove_query();
        auto out = u.params();

        for (auto const& [k, v] : params) {
            out.append(boost::urls::param_view{k, v});
        }
    }

    return std::string(u.buffer());
}

std::optional<std::string> UrlService::extractDomain(std::string_view normalizedUrl) {
    auto parsed = urls::parse_absolute_uri(normalizedUrl);
    if (!parsed) [[unlikely]] {
        Log::err("Invalid normalized URL");
        return std::nullopt;
    }

    return std::string(parsed->host());
}

std::optional<std::string> UrlService::extractPath(std::string_view normalizedUrl) {
    auto parsed = urls::parse_absolute_uri(normalizedUrl);
    if (!parsed) [[unlikely]] {
        Log::err("Invalid normalized URL");
        return std::nullopt;
    }

    return std::string(parsed->path());
}

std::optional<UrlService::UrlParts> UrlService::getUrlParts(std::string_view normalizedUrl) {
    auto res = urls::parse_uri(normalizedUrl);
    if (!res) [[unlikely]] {
        Log::err("Failed to parse normalized URL: {}", normalizedUrl);
        return std::nullopt;
    }

    return UrlParts{.domain = std::string(res->host()), .path = std::string(res->path())};
}
