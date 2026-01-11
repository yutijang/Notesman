#include <cstddef>
#include <cstdint>
#include <string>
#include <array>
#include <format>
#include <fstream>
#include <ios>
#include <filesystem>

#include "helper.hpp"
#include "model.hpp"

namespace Utils {
    std::string getExtensionFromDownloadUrl(const std::string &url) {
        std::size_t lastSlash = url.find_last_of('/');
        if (lastSlash == std::string::npos) {
            lastSlash = 0;
        } else {
            lastSlash += 1;
        }

        std::string filenamePart = url.substr(lastSlash);

        std::size_t nonFilenameCharPos = filenamePart.find_first_of("?#");
        if (nonFilenameCharPos != std::string::npos) { filenamePart.erase(nonFilenameCharPos); }

        std::size_t extPos = filenamePart.find('.');

        if (extPos != std::string::npos && extPos < filenamePart.length() - 1) {
            return filenamePart.substr(extPos + 1);
        }

        return {};
    }

    // NOLINTBEGIN
    std::string normalizationDBFileSize(std::uint64_t size) {
        if (size == 0) { return "0 B"; }

        constexpr std::array symbol{"B", "KB", "MB", "GB"};

        double tmpSize{static_cast<double>(size)};
        std::size_t count{};

        while (tmpSize >= 1024.0 && count < symbol.size() - 1) {
            tmpSize /= 1024.0;
            count++;
        }

        std::string formatted = (tmpSize >= 100.0)
                                  ? std::format("{:.0f} {}", tmpSize, symbol[count])
                                  : std::format("{:.2f} {}", tmpSize, symbol[count]);

        std::size_t space_pos = formatted.find(' ');
        if (space_pos != std::string::npos) {
            std::string num_part = formatted.substr(0, space_pos);
            std::size_t dot_pos = num_part.find('.');
            if (dot_pos != std::string::npos) {
                std::size_t last = num_part.size() - 1;
                while (last > dot_pos && num_part[last] == '0') { --last; }
                if (num_part[last] == '.') { last--; }
                num_part.erase(last + 1);
            }
            formatted = num_part + formatted.substr(space_pos);
        }

        return formatted;
    }

    // NOLINTEND

    std::string readFileToString(const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) { return {}; }

        file.seekg(0, std::ios::end);
        const std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::string content;
        content.resize(static_cast<std::size_t>(size));

        file.read(content.data(), size);

        return content;
    }

    IndexableResult isIndexable(ResourceType type, const std::filesystem::path &path) {
        if (type != ResourceType::cCppCode && type != ResourceType::htmlDoc) {
            return IndexableResult::noUnsupportedType;
        }

        std::error_code ec;
        const auto fileSize = std::filesystem::file_size(path, ec);
        if (ec || fileSize == 0 ||
            fileSize >
                static_cast<uintmax_t>(2 * 1024 * 1024)) { // NOLINT(readability-magic-numbers)
            return IndexableResult::noTooLarge;
        }

        std::ifstream file(path, std::ios::binary);
        if (!file) { return IndexableResult::noFileAccess; }

        char buffer[4096]; // NOLINT(readability-magic-numbers)
        file.read(buffer, sizeof(buffer));
        const auto bytesRead = file.gcount();

        int suspicious{};
        for (int i = 0; i < bytesRead; ++i) {
            auto c = static_cast<unsigned char>(buffer[i]);

            if (c == '\0') { return IndexableResult::noBinaryDetected; }

            if (c < 7 || (c > 13 && c < 32)) { // NOLINT(readability-magic-numbers)
                if (++suspicious > 8) {        // ~0.2%  // NOLINT(readability-magic-numbers)
                    return IndexableResult::noBinaryDetected;
                }
            }
        }

        return IndexableResult::yes;
    }

    std::string joinTags(const std::vector<std::string> &tags) {
        std::string result;
        bool first{true};

        for (const auto &tag : tags) {
            if (tag.empty()) { continue; }

            if (!first) { result.append(", "); }
            first = false;

            result.append("#");
            result.append(tag);
        }

        return result;
    }
} // namespace Utils
