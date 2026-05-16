#include "application/AppUIApplier.hpp"

#include "gui/UiConstants.hpp"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QPalette>
#include <QString>
#include <QStyleFactory>
#include <QTranslator>
#include <memory>

namespace AppUI {

void applyTheme(UiConst::Theme theme) {
    QString qssPath;
    QColor linkColor;

    switch (theme) {
        case UiConst::Theme::Light:
            qssPath = ":/qss/light.qss";
            linkColor = QColor("#0000EE");
            break;
        case UiConst::Theme::Dark:
            qssPath = ":/qss/dark.qss";
            linkColor = QColor("#4FC3F7");
            break;
    }

    QPalette palette = qApp->palette();
    palette.setColor(QPalette::Link, linkColor);
    qApp->setPalette(palette);

    QFile qssFile(qssPath);
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        QString const styleSheet = QString::fromUtf8(qssFile.readAll());
        qApp->setStyleSheet(styleSheet);
        qssFile.close();
    } else {
        qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setStyleSheet("");
    }

    qApp->setFont(qApp->font());
}

void applyLanguage(UiConst::Language lang, std::unique_ptr<QTranslator>& translator) {
    if (translator) {
        qApp->removeTranslator(translator.get());
    }

    if (lang == UiConst::Language::Vietnamese) {
        translator = std::make_unique<QTranslator>();
        if (translator->load(":/i18n/app_vi.qm")) {
            qApp->installTranslator(translator.get());
        } else {
            translator.reset();
        }
    } else {
        translator.reset();
    }

    QEvent event(QEvent::LanguageChange);
    QCoreApplication::sendEvent(qApp, &event);
}

} // namespace AppUI
