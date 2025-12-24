#include <cstdint>
#include <string>
#include <array>
#include <format>

#include "helper.hpp"

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

        size_t space_pos = formatted.find(' ');
        if (space_pos != std::string::npos) {
            std::string num_part = formatted.substr(0, space_pos);
            size_t dot_pos = num_part.find('.');
            if (dot_pos != std::string::npos) {
                size_t last = num_part.size() - 1;
                while (last > dot_pos && num_part[last] == '0') { --last; }
                if (num_part[last] == '.') { last--; }
                num_part.erase(last + 1);
            }
            formatted = num_part + formatted.substr(space_pos);
        }

        return formatted;
    }

    // NOLINTEND
} // namespace Utils
