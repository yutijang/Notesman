#include <QToolButton>
#include <QMenu>
#include <QWidget>
#include <QFont>
#include <QPoint>
#include <QList>
#include <QMouseEvent>
#include <QSize>
#include <Qt>
#include <QObject>

#include "InfoCornerWidget.hpp"

namespace {
    constexpr auto INFO_ICON_SIZE = QSize(18, 18);
} // namespace

InfoCornerWidget::InfoCornerWidget(QWidget* parent) : QToolButton(parent) {
    setObjectName("infoCornerButton");
    setIcon(QIcon(":/icons/info.ico"));
    setAutoRaise(true);
    setToolTip(tr("Updates and About"));
    setIconSize(INFO_ICON_SIZE);
    setCursor(Qt::PointingHandCursor);

    auto* menu = new QMenu(this);
    menu->addAction(QIcon(":/icons/update.ico"), tr("Check for updates"), this,
                    &InfoCornerWidget::onCheckUpdate);
    menu->addSeparator();
    menu->addAction(QIcon(":/icons/about.ico"), tr("About"), this, &InfoCornerWidget::onAbout);
    setMenu(menu);
    setPopupMode(QToolButton::InstantPopup);
}

void InfoCornerWidget::onCheckUpdate() {
    Q_EMIT checkUpdateRequested();
}

void InfoCornerWidget::onAbout() {
    Q_EMIT aboutRequested();
}

void InfoCornerWidget::mousePressEvent(QMouseEvent* event) {
    if ((menu() != nullptr) && event->button() == Qt::LeftButton) {
        QPoint globalPos = mapToGlobal(event->pos());
        int menuWidth = menu()->sizeHint().width();
        QPoint anchor(globalPos.x() - menuWidth, globalPos.y());
        menu()->popup(anchor);
        event->accept();
        return;
    }
    QToolButton::mousePressEvent(event); // Fallback default
}

void InfoCornerWidget::retranslateUi() {
    this->setToolTip(tr("Updates and About"));

    if (this->menu() != nullptr) {
        QList<QAction*> actions = this->menu()->actions();
        if (actions.size() >= 3) {
            actions[0]->setText(tr("Check for updates"));
            actions[2]->setText(tr("About"));
        }
    }
}
