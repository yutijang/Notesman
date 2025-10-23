#include <QApplication>
#include <QFontDatabase>
#include <QFont>
#include <QDebug>
#include <QString>
#include <QStringList>
#include "FontLoader.hpp"

void FontLoader::loadCustomFontOnce() {
    static bool loaded = false; // đảm bảo chỉ chạy 1 lần
    if (loaded) { return; }
    loaded = true;

    const QString fontPath = ":/fonts/Roboto-Condensed-webfont.ttf";
    const int fontId = QFontDatabase::addApplicationFont(fontPath);
    if (fontId == -1) {
        qWarning() << "Failed to load custom font from" << fontPath;
        return;
    }

    const QStringList loadedFamilies = QFontDatabase::applicationFontFamilies(fontId);
    if (!loadedFamilies.isEmpty()) {
        QFont appFont(loadedFamilies.at(0));
        appFont.setPointSize(11); // NOLINT(readability-magic-numbers)
        qApp->setFont(appFont);
        // qDebug() << "Custom font applied:" << appFont.family();
    } else {
        qWarning() << "No font families found in" << fontPath;
    }

    QFontDatabase::addApplicationFont(":/fonts/JetBrainsMono-Regular.ttf");
}
