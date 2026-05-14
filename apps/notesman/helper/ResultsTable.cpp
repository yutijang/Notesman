#include "helper/ResultsTable.hpp"

#include <QApplication>
#include <QFocusEvent>
#include <QFont>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QMouseEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>

ResultsTable::ResultsTable(QWidget* parent) : QTableWidget(parent) {}

void ResultsTable::mousePressEvent(QMouseEvent* event) {
    QModelIndex idx = indexAt(event->pos());
    if (!idx.isValid()) {
        clearSelection();
        selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
    }
    QTableWidget::mousePressEvent(event);
}

void ResultsTable::focusOutEvent(QFocusEvent* event) {
    QTableWidget::focusOutEvent(event);
    viewport()->update();
}

void ResultsTable::showEvent(QShowEvent* event) {
    QTableWidget::showEvent(event);

    updateLastHeaderBorder();

    // Dùng timer để clear sau khi Qt hoàn tất khôi phục selection mặc định
    QTimer::singleShot(0, this, [this]() {
        selectionModel()->clearSelection();
        selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
        viewport()->update();
    });
}

void ResultsTable::changeEvent(QEvent* event) {
    if (event->type() == QEvent::ApplicationFontChange) {
        QFont const f = qApp->font();
        setFont(f);
        if (horizontalHeader() != nullptr) {
            horizontalHeader()->setFont(f);
        }
        if (verticalHeader() != nullptr) {
            verticalHeader()->setFont(f);
        }
        for (int row = 0; row < rowCount(); ++row) {
            for (int col = 0; col < columnCount(); ++col) {
                if (auto* item = this->item(row, col)) {
                    item->setFont(f);
                }
            }
        }
        viewport()->update();
    }
    QTableWidget::changeEvent(event);
}

void ResultsTable::resizeEvent(QResizeEvent* event) {
    QTableWidget::resizeEvent(event);

    updateLastHeaderBorder();
}

void ResultsTable::updateLastHeaderBorder() {
    auto* hHeader = horizontalHeader();
    auto* vScrollBar = verticalScrollBar();
    if ((hHeader == nullptr) || (vScrollBar == nullptr)) {
        return;
    }

    bool isVisible = vScrollBar->isVisible();

    hHeader->setProperty("show-last-border", isVisible);

    hHeader->style()->unpolish(hHeader);
    hHeader->style()->polish(hHeader);
    hHeader->update();
}
