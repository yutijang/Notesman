#include <string>

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

        return "";
    }
} // namespace Utils
