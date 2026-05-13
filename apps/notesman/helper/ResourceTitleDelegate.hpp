#pragma once

#include "model.hpp"

#include <QModelIndex>
#include <QSize>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QSvgRenderer>
#include <memory>
#include <unordered_map>

class QPainter;

class ResourceTitleDelegate final : public QStyledItemDelegate {
    Q_OBJECT

  public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter,
               QStyleOptionViewItem const& option,
               QModelIndex const& index) const override;

    [[nodiscard]] QSize sizeHint(QStyleOptionViewItem const& option,
                                 QModelIndex const& index) const override;

  private:
    QSvgRenderer* getRenderer(ResourceType type) const;

    mutable std::unordered_map<ResourceType, std::unique_ptr<QSvgRenderer>> m_rendererCache;
};
