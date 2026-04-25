#include "FontLoader.hpp"

#include "Logger.hpp"
#include "UiConstants.hpp"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QStringList>

void FontLoader::loadCustomFontOnce() {
    static bool loaded{}; // đảm bảo chỉ chạy 1 lần
    if (loaded) { return; }
    loaded = true;

    QString const fontPath = ":/fonts/Roboto-Condensed-webfont.ttf";
    int const fontId = QFontDatabase::addApplicationFont(fontPath);
    if (fontId == -1) {
        Log::warn("Failed to load custom font from {}", fontPath.toStdString());
        return;
    }

    QStringList const loadedFamilies = QFontDatabase::applicationFontFamilies(fontId);
    if (!loadedFamilies.isEmpty()) {
        QFont appFont(loadedFamilies.at(0));
        appFont.setPointSize(UiConst::FONT_SIZE); // NOLINT(readability-magic-numbers)
        qApp->setFont(appFont);
    } else {
        Log::warn("No font families found in {}", fontPath.toStdString());
    }

    QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Regular.ttf");
}
