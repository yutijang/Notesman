#pragma once

#include <QMessageBox>
#include <QString>

class QWidget;

class DialogUtils final {
  public:
    DialogUtils() = delete;

    static QMessageBox::StandardButton
        showInfo(QWidget* parent, QString const& title, QString const& text, bool isRich = false);
    static QMessageBox::StandardButton showWarning(QWidget* parent,
                                                   QString const& title,
                                                   QString const& text,
                                                   bool isRich = false);
    static QMessageBox::StandardButton
        showError(QWidget* parent, QString const& title, QString const& text, bool isRich = false);
    static QMessageBox::StandardButton showQuestion(QWidget* parent,
                                                    QString const& title,
                                                    QString const& text,
                                                    bool isRich = false);
    static QMessageBox::StandardButton showCustom(QWidget* parent,
                                                  QMessageBox::Icon icon,
                                                  QString const& title,
                                                  QString const& text,
                                                  QMessageBox::StandardButtons buttons,
                                                  bool isRich = false);

  private:
    static void centerDialog(QWidget* parent, QMessageBox& box);
};
