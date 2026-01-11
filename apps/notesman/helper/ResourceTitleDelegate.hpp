#pragma once

#include <memory>
#include <unordered_map>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QSize>
#include <QSvgRenderer>

#include "model.hpp"

class QPainter;

class ResourceTitleDelegate final : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter* painter, const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

        [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const override;

    private:
        QSvgRenderer* getRenderer(ResourceType type) const;

        mutable std::unordered_map<ResourceType, std::unique_ptr<QSvgRenderer>> m_rendererCache;
};
