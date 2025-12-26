#pragma once

#include <QStyledItemDelegate>
#include <QSize>
#include <QModelIndex>
#include <QStyleOptionViewItem>

class QPainter;

class ResourceTitleDelegate final : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter* painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

        [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const override;
};
