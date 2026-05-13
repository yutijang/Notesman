#include "helper.hpp"

#include "model.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace Utils {

std::string getExtensionFromDownloadUrl(std::string const& url) {
    std::size_t lastSlash = url.find_last_of('/');
    if (lastSlash == std::string::npos) {
        lastSlash = 0;
    } else {
        lastSlash += 1;
    }

    std::string filenamePart = url.substr(lastSlash);

    std::size_t nonFilenameCharPos = filenamePart.find_first_of("?#");
    if (nonFilenameCharPos != std::string::npos) {
        filenamePart.erase(nonFilenameCharPos);
    }

    std::size_t extPos = filenamePart.find('.');

    if (extPos != std::string::npos && extPos < filenamePart.length() - 1) {
        return filenamePart.substr(extPos + 1);
    }

    return {};
}

// NOLINTBEGIN
std::string normalizationDBFileSize(std::uint64_t size) {
    if (size == 0) {
        return "0 B";
    }

    constexpr std::array symbol{"B", "KB", "MB", "GB"};

    double tmpSize{static_cast<double>(size)};
    std::size_t count{};

    while (tmpSize >= 1024.0 && count < symbol.size() - 1) {
        tmpSize /= 1024.0;
        count++;
    }

    std::string formatted = (tmpSize >= 100.0) ? std::format("{:.0f} {}", tmpSize, symbol[count])
                                               : std::format("{:.2f} {}", tmpSize, symbol[count]);

    std::size_t space_pos = formatted.find(' ');
    if (space_pos != std::string::npos) {
        std::string num_part = formatted.substr(0, space_pos);
        std::size_t dot_pos = num_part.find('.');
        if (dot_pos != std::string::npos) {
            std::size_t last = num_part.size() - 1;
            while (last > dot_pos && num_part[last] == '0') {
                --last;
            }
            if (num_part[last] == '.') {
                last--;
            }
            num_part.erase(last + 1);
        }
        formatted = num_part + formatted.substr(space_pos);
    }

    return formatted;
}

// NOLINTEND

std::string readFileToString(std::filesystem::path const& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }

    file.seekg(0, std::ios::end);
    std::streamsize const size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string content;
    content.resize(static_cast<std::size_t>(size));

    file.read(content.data(), size);

    return content;
}

IndexableResult isIndexable(ResourceType type, std::filesystem::path const& path) {
    if (type == ResourceType::PdfDoc || type == ResourceType::EpubDoc) {
        return IndexableResult::NoUnsupportedType;
    }

    std::error_code ec;
    auto const fileSize = std::filesystem::file_size(path, ec);
    if (ec || fileSize == 0 ||
        fileSize > static_cast<uintmax_t>(2 * 1024 * 1024)) { // NOLINT(readability-magic-numbers)
        return IndexableResult::NoTooLarge;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return IndexableResult::NoFileAccess;
    }

    std::array<char, 4096> buffer{}; // NOLINT(readability-magic-numbers)
    file.read(buffer.data(), buffer.size());
    auto const bytesRead = file.gcount();

    std::string_view dataView(buffer.data(), static_cast<std::size_t>(bytesRead));

    int suspicious{};
    for (char rawChar : dataView) {
        auto c = static_cast<unsigned char>(rawChar);

        if (c == '\0') {
            return IndexableResult::NoBinaryDetected;
        }

        if (c < 7 || (c > 13 && c < 32)) { // NOLINT(readability-magic-numbers)
            if (++suspicious > 8) {        // ~0.2%  // NOLINT(readability-magic-numbers)
                return IndexableResult::NoBinaryDetected;
            }
        }
    }

    return IndexableResult::Yes;
}

std::string joinTags(std::vector<std::string> const& tags) {
    std::string result;
    bool first{true};

    for (auto const& tag : tags) {
        if (tag.empty()) {
            continue;
        }

        if (!first) {
            result.append(", ");
        }
        first = false;

        result.append("#");
        result.append(tag);
    }

    return result;
}

// NOLINTBEGIN (readability-magic-numbers)
[[nodiscard]] bool looksLikeCppCode(std::string_view text) noexcept {
    if (text.size() < 30) {
        return false;
    }

    int score{};

    if (text.contains("#include <") || text.contains("#include \"")) {
        score += 3;
        if (score >= 5) {
            return true;
        }
    }
    if (text.contains("namespace ") || text.contains("class ") || text.contains("struct ")) {
        score += 3;
        if (score >= 5) {
            return true;
        }
    }
    if (text.contains("::")) {
        score += 2;
        if (score >= 5) {
            return true;
        }
    }
    if (text.contains("->")) {
        score += 1;
        if (score >= 5) {
            return true;
        }
    }

    return score >= 5;
}

// NOLINTEND

void trimS(std::string& source) {
    auto isSpace = [](unsigned char ch) {
        return std::isspace(ch);
    };
    auto trimmedView = source | std::views::drop_while(isSpace) | std::views::reverse |
                       std::views::drop_while(isSpace) | std::views::reverse;
    source = {trimmedView.begin(), trimmedView.end()};
}

std::string sanitizeFtsQuery(std::string_view input, bool wrapInQuotes) {
    if (input.empty()) {
        return {};
    }

    std::string clean{input};
    bool hasWildcard{};

    // 1. Kiểm tra dấu *
    if (clean.back() == '*') {
        hasWildcard = true;
        clean.pop_back();
    }

    // 2. Xóa nháy kép
    std::erase(clean, '\"');
    if (clean.empty()) {
        return {};
    }

    // 3. Đóng gói
    if (wrapInQuotes) {
        // Mode Title: Bọc nháy kép toàn bộ để tìm chính xác cụm từ
        return "\"" + clean + (hasWildcard ? "*\"" : "\"");
    }

    // Mode Content: Không bọc nháy kép để tìm linh hoạt (từng từ rời rạc)
    // Nhưng vẫn thêm * nếu người dùng yêu cầu
    return clean + (hasWildcard ? "*" : "");
}

std::string normalizeSnippet(std::string_view input) {
    std::string out;
    out.reserve(input.size());

    bool inWhitespace = false;

    for (char ch : input) {
        auto c = static_cast<unsigned char>(ch);
        if (std::isspace(c) != 0) {
            if (!inWhitespace) {
                out.push_back(' ');
                inWhitespace = true;
            }
        } else {
            out.push_back(static_cast<char>(c));
            inWhitespace = false;
        }
    }

    // trim đầu/trim cuối
    trimS(out);

    return out;
}

std::string toLikeQuery(std::string_view input) {
    if (input.empty()) {
        return {};
    }

    std::string clean{input};
    std::erase(clean, '\"');

    trimS(clean);

    return clean + "%"; // Tìm kiếm theo kiểu "bắt đầu bằng"
}

} // namespace Utils
