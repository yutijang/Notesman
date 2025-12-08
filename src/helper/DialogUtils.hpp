#pragma once

#include <QMessageBox>
#include <QString>

class QWidget;

class DialogUtils final {
    public:
        DialogUtils() = delete;

        static QMessageBox::StandardButton showInfo(QWidget* parent, const QString &title,
                                                    const QString &text, bool isRich = false);
        static QMessageBox::StandardButton showWarning(QWidget* parent, const QString &title,
                                                       const QString &text, bool isRich = false);
        static QMessageBox::StandardButton showError(QWidget* parent, const QString &title,
                                                     const QString &text, bool isRich = false);
        static QMessageBox::StandardButton showQuestion(QWidget* parent, const QString &title,
                                                        const QString &text, bool isRich = false);
        static QMessageBox::StandardButton showCustom(QWidget* parent, QMessageBox::Icon icon,
                                                      const QString &title, const QString &text,
                                                      QMessageBox::StandardButtons buttons,
                                                      bool isRich = false);

    private:
        static void centerDialog(QWidget* parent, QMessageBox &box);
};
