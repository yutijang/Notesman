#pragma once

#include <QString>
#include <optional>
#include <vector>

class EpubResolver {
    public:
        // Nhận path .epub (absolute hoặc relative)
        // Trả về absolute path tới __entry.html nếu thành công
        static std::optional<QString> resolveToHtml(QString const& epubPath);

        EpubResolver() = delete;

        struct OpfData {
                QString baseDir;
                std::vector<QString> spineFiles;
        };
};
