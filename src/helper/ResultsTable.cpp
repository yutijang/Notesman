#include <QMouseEvent>
#include <QFocusEvent>
#include <QTimer>
#include "ResultsTable.hpp"

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
    clearSelection();
    selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
    QTableWidget::focusOutEvent(event);
}

void ResultsTable::showEvent(QShowEvent* event) {
    QTableWidget::showEvent(event);

    // Dùng timer để clear sau khi Qt hoàn tất khôi phục selection mặc định
    QTimer::singleShot(0, this, [this]() {
        selectionModel()->clearSelection();
        selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
        viewport()->update();
    });
}
