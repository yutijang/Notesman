#include <QtCore>
#include <qlabel.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qsize.h>
#include <qstyle.h>

#include "DialogUtils.hpp"

void DialogUtils::centerDialog(QWidget* parent, QMessageBox &box) {
    box.ensurePolished();
    box.adjustSize();

    QWidget* root = (parent != nullptr) ? parent->window() : nullptr;
    if (root == nullptr) { return; }

    const QSize dlgClientSize = box.size();

    QStyle* style = box.style();
    const int titleBarH = style->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, &box);
    const int frameW = style->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, &box);

    const int dlgTotalW = dlgClientSize.width() + (2 * frameW);
    const int dlgTotalH = dlgClientSize.height() + titleBarH + (2 * frameW);

    const QRect parentFrame = root->frameGeometry();

    const int x = parentFrame.center().x() - (dlgTotalW / 2);
    const int y = parentFrame.center().y() - (dlgTotalH / 2);

    box.move(x, y);
}

QMessageBox::StandardButton DialogUtils::showInfo(QWidget* parent, const QString &title,
                                                  const QString &text, bool isRich) {
    return showCustom(parent, QMessageBox::Information, title, text, QMessageBox::Ok, isRich);
}

QMessageBox::StandardButton DialogUtils::showWarning(QWidget* parent, const QString &title,
                                                     const QString &text, bool isRich) {
    return showCustom(parent, QMessageBox::Warning, title, text, QMessageBox::Ok, isRich);
}

QMessageBox::StandardButton DialogUtils::showError(QWidget* parent, const QString &title,
                                                   const QString &text, bool isRich) {
    return showCustom(parent, QMessageBox::Critical, title, text, QMessageBox::Ok, isRich);
}

QMessageBox::StandardButton DialogUtils::showQuestion(QWidget* parent, const QString &title,
                                                      const QString &text, bool isRich) {
    return showCustom(parent, QMessageBox::Question, title, text,
                      QMessageBox::Yes | QMessageBox::No, isRich);
}

QMessageBox::StandardButton DialogUtils::showCustom(QWidget* parent, QMessageBox::Icon icon,
                                                    const QString &title, const QString &text,
                                                    QMessageBox::StandardButtons buttons,
                                                    bool isRich) {
    QMessageBox box(icon, title, text, buttons, parent);
    box.setWindowModality(Qt::WindowModal);
    box.setDefaultButton(QMessageBox::No);

    if (isRich) { box.setTextFormat(Qt::RichText); }

    auto* msgLabel = box.findChild<QLabel*>("qt_msgbox_label");
    if (msgLabel != nullptr) { msgLabel->setStyleSheet("padding: 10px 30px 30px 10px;"); }

    centerDialog(parent, box);

    return static_cast<QMessageBox::StandardButton>(box.exec());
}
