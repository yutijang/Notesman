#pragma once

#include <QString>

class MarkdownToHtml {
    public:
        static QString convertFileToHtml(QString const& mdPath, bool isDarkTheme);
};
