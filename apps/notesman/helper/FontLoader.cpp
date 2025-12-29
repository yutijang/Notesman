#include <QApplication>
#include <QFontDatabase>
#include <QFont>
#include <QString>
#include <QStringList>

#include "FontLoader.hpp"
#include "Logger.hpp"
#include "UiConstants.hpp"

void FontLoader::loadCustomFontOnce() {
    static bool loaded{}; // đảm bảo chỉ chạy 1 lần
    if (loaded) { return; }
    loaded = true;

    const QString fontPath = ":/fonts/Roboto-Condensed-webfont.ttf";
    const int fontId = QFontDatabase::addApplicationFont(fontPath);
    if (fontId == -1) {
        Log::warn("Failed to load custom font from {}", fontPath.toStdString());
        return;
    }

    const QStringList loadedFamilies = QFontDatabase::applicationFontFamilies(fontId);
    if (!loadedFamilies.isEmpty()) {
        QFont appFont(loadedFamilies.at(0));
        appFont.setPointSize(UiConst::FONT_SIZE); // NOLINT(readability-magic-numbers)
        qApp->setFont(appFont);
    } else {
        Log::warn("No font families found in {}", fontPath.toStdString());
    }

    QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Regular.ttf");
}
