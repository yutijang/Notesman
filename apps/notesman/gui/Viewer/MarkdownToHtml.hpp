#pragma once

#include <QString>

class MarkdownToHtml {
    public:
        static QString convertFileToHtml(const QString &mdPath, bool isDarkTheme);
};
