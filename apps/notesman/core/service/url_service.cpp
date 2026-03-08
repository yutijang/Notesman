#include "url_service.hpp"

#include "Logger.hpp"
#include "model.hpp"

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

/**
 * @brief Kiểm tra URL hợp lệ cơ bản (Simplified RFC 3986)
 * Complexity: O(N) với N là chiều dài chuỗi.
 */
/*
bool UrlService::isValidUrl(std::string_view urlRaw) {
    if (urlRaw.size() < 8 || urlRaw.size() > 2048) { return false; }

    // 1. Kiểm tra Scheme (RFC 3986: scheme = alpha *( alpha / digit / "+" / "-" / "." ))
    auto schemeEnd = urlRaw.find("://");
    if (schemeEnd == std::string_view::npos || schemeEnd == 0) { return false; }

    std::string_view scheme = urlRaw.substr(0, schemeEnd);
    auto isAlpha = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
    if (!isAlpha(scheme[0])) { return false; }

    for (char c : scheme) {
        if (!isAlpha(c) && (c < '0' || c > '9') && c != '+' && c != '-' && c != '.') {
            return false;
        }
    }

    // 2. Tách Authority (Userinfo + Host + Port)
    std::string_view rest = urlRaw.substr(schemeEnd + 3);
    if (rest.empty()) { return false; }

    auto authorityEnd = rest.find_first_of("/?#");
    std::string_view authority =
        (authorityEnd == std::string_view::npos) ? rest : rest.substr(0, authorityEnd);
    if (authority.empty()) { return false; }

    // 3. Loại bỏ Userinfo nếu có (user:password@)
    if (auto atPos = authority.find('@'); atPos != std::string_view::npos) {
        authority = authority.substr(atPos + 1);
        if (authority.empty()) {
            return false; // Tránh trường hợp "user@"
        }
    }

    // 4. Xử lý Host và Port
    std::string_view host = authority;
    auto validatePort = [](std::string_view p) {
        if (p.empty() || p.size() > 5) { return false; }

        std::uint32_t port = 0;
        auto [ptr, ec] = std::from_chars(p.data(), p.data() + p.size(), port);

        if (ec != std::errc{} || ptr != p.data() + p.size()) { return false; }
        if (port == 0 || port > 65535) { return false; }

        return true;
    };

    if (host[0] == '[') { // IPv6
        auto closeBracket = host.find(']');
        if (closeBracket == std::string_view::npos) { return false; }

        std::string_view ipPart = host.substr(1, closeBracket - 1);
        if (ipPart.empty()) { return false; }

        for (char c : ipPart) {
            if ((std::isxdigit(static_cast<unsigned char>(c)) == 0) && c != ':') { return false; }
        }

        if (closeBracket + 1 < host.size()) {
            if (host[closeBracket + 1] != ':') { return false; }
            if (!validatePort(host.substr(closeBracket + 2))) { return false; }
        }
        host = ipPart;
    } else {
        if (auto colon = host.find(':'); colon != std::string_view::npos) {
            if (!validatePort(host.substr(colon + 1))) { return false; }
            host = host.substr(0, colon);
        }
    }

    // 5. Kiểm tra tính hợp lệ của Hostname
    if (host.empty() || host.front() == '.' || host.back() == '.' || host.front() == '-' ||
        host.back() == '-' || host.find("..") != std::string_view::npos) {
        return false;
    }

    auto isInvalidHostChar = [](char c) {
        return (c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && (c < '0' || c > '9') && c != '-' &&
               c != '.';
    };

    // Nếu không phải IPv6 (đã tách ở trên) thì kiểm tra ký tự DNS
    return authority[0] == '[' || !std::ranges::any_of(host, isInvalidHostChar);
}
*/

std::optional<sqlite3_int64> UrlService::addUrlResource(std::string_view title, ResourceType type,
                                                        std::string_view rawUrl) {
    auto normalizedUrl = normalizeUrl(rawUrl);
    if (!normalizedUrl) { return std::nullopt; }

    if (auto resIdOpt = m_urlRepo.getResourceIdByNormalizedUrl(*normalizedUrl)) { return resIdOpt; }

    // NOLINTNEXTLINE (-Wmissing-designated-field-initializers)
    sqlite3_int64 resourceId = m_resRepo.insert({.title = std::string(title), .type = type});
    auto partsOpt = getUrlParts(*normalizedUrl);
    if (!partsOpt) { return std::nullopt; }

    m_urlRepo.insertUrl(resourceId, rawUrl, *normalizedUrl, partsOpt->domain, partsOpt->path);

    return resourceId;
}

void UrlService::updateUrl(sqlite3_int64 resourceId, std::string_view rawUrl) {
    auto normalizedUrl = normalizeUrl(rawUrl);
    if (!normalizedUrl) {
        Log::err("Invalid URL");
        return;
    }

    auto partsOpt = getUrlParts(*normalizedUrl);
    if (!partsOpt) {
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
    if (!parsed) {
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

        for (auto const& p : view) { params.emplace_back(p.key, p.value); }

        std::ranges::sort(params);

        u.remove_query();
        auto out = u.params();

        for (auto const& [k, v] : params) { out.append(boost::urls::param_view{k, v}); }
    }

    return std::string(u.buffer());
}

std::optional<std::string> UrlService::extractDomain(std::string_view normalizedUrl) {
    auto parsed = urls::parse_absolute_uri(normalizedUrl);
    if (!parsed) {
        Log::err("Invalid normalized URL");
        return std::nullopt;
    }

    return std::string(parsed->host());
}

std::optional<std::string> UrlService::extractPath(std::string_view normalizedUrl) {
    auto parsed = urls::parse_absolute_uri(normalizedUrl);
    if (!parsed) {
        Log::err("Invalid normalized URL");
        return std::nullopt;
    }

    return std::string(parsed->path());
}

std::optional<UrlService::UrlParts> UrlService::getUrlParts(std::string_view normalizedUrl) {
    auto res = urls::parse_uri(normalizedUrl);
    if (!res) {
        Log::err("Failed to parse normalized URL: {}", normalizedUrl);
        return std::nullopt;
    }

    return UrlParts{.domain = std::string(res->host()), .path = std::string(res->path())};
}
