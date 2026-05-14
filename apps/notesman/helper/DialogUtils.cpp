#include "helper/DialogUtils.hpp"

#include <QLabel>
#include <QMessageBox>
#include <QPoint>
#include <QSize>
#include <QStyle>
#include <QWidget>
#include <Qt>
#include <QtCore>

void DialogUtils::centerDialog(QWidget* parent, QMessageBox& box) {
    box.ensurePolished();
    box.adjustSize();

    QWidget* root = (parent != nullptr) ? parent->window() : nullptr;
    if (root == nullptr) {
        return;
    }

    QSize const dlgClientSize = box.size();

    QStyle* style = box.style();
    int const titleBarH = style->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, &box);
    int const frameW = style->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, &box);

    int const dlgTotalW = dlgClientSize.width() + (2 * frameW);
    int const dlgTotalH = dlgClientSize.height() + titleBarH + (2 * frameW);

    QRect const parentFrame = root->frameGeometry();

    int const x = parentFrame.center().x() - (dlgTotalW / 2);
    int const y = parentFrame.center().y() - (dlgTotalH / 2);

    box.move(x, y);
}

QMessageBox::StandardButton
    DialogUtils::showInfo(QWidget* parent, QString const& title, QString const& text, bool isRich) {
    return showCustom(parent, QMessageBox::Information, title, text, QMessageBox::Ok, isRich);
}

QMessageBox::StandardButton DialogUtils::showWarning(QWidget* parent,
                                                     QString const& title,
                                                     QString const& text,
                                                     bool isRich) {
    return showCustom(parent, QMessageBox::Warning, title, text, QMessageBox::Ok, isRich);
}

QMessageBox::StandardButton DialogUtils::showError(QWidget* parent,
                                                   QString const& title,
                                                   QString const& text,
                                                   bool isRich) {
    return showCustom(parent, QMessageBox::Critical, title, text, QMessageBox::Ok, isRich);
}

QMessageBox::StandardButton DialogUtils::showQuestion(QWidget* parent,
                                                      QString const& title,
                                                      QString const& text,
                                                      bool isRich) {
    return showCustom(
        parent, QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No, isRich);
}

QMessageBox::StandardButton DialogUtils::showCustom(QWidget* parent,
                                                    QMessageBox::Icon icon,
                                                    QString const& title,
                                                    QString const& text,
                                                    QMessageBox::StandardButtons buttons,
                                                    bool isRich) {
    QMessageBox box(icon, title, text, buttons, parent);
    box.setWindowModality(Qt::WindowModal);
    box.setDefaultButton(QMessageBox::No);

    if (isRich) {
        box.setTextFormat(Qt::RichText);
    }

    auto* msgLabel = box.findChild<QLabel*>("qt_msgbox_label");
    if (msgLabel != nullptr) {
        msgLabel->setStyleSheet("padding: 10px 30px 30px 10px;");
    }

    centerDialog(parent, box);

    return static_cast<QMessageBox::StandardButton>(box.exec());
}
