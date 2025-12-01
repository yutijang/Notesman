#include <QWidget>
#include <QtCore>
#include <QPoint>
#include <QSize>
#include <QStyle>
#include <QMessageBox>

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
                                                  const QString &text) {
    return showCustom(parent, QMessageBox::Information, title, text, QMessageBox::Ok);
}

QMessageBox::StandardButton DialogUtils::showWarning(QWidget* parent, const QString &title,
                                                     const QString &text) {
    return showCustom(parent, QMessageBox::Warning, title, text, QMessageBox::Ok);
}

QMessageBox::StandardButton DialogUtils::showError(QWidget* parent, const QString &title,
                                                   const QString &text) {
    return showCustom(parent, QMessageBox::Critical, title, text, QMessageBox::Ok);
}

QMessageBox::StandardButton DialogUtils::showQuestion(QWidget* parent, const QString &title,
                                                      const QString &text) {
    return showCustom(parent, QMessageBox::Question, title, text,
                      QMessageBox::Yes | QMessageBox::No);
}

QMessageBox::StandardButton DialogUtils::showCustom(QWidget* parent, QMessageBox::Icon icon,
                                                    const QString &title, const QString &text,
                                                    QMessageBox::StandardButtons buttons) {
    QMessageBox box(icon, title, text, buttons, parent);
    box.setWindowModality(Qt::WindowModal);
    box.setDefaultButton(QMessageBox::No);

    centerDialog(parent, box);

    return static_cast<QMessageBox::StandardButton>(box.exec());
}
