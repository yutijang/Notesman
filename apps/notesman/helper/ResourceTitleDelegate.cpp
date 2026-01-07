#include <QPainter>
#include <QApplication>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <Qt>
#include <QString>
#include <QRect>
#include <QSize>
#include <QStyledItemDelegate>
#include <QPalette>
#include <QSvgRenderer>
#include <qminmax.h>

#include "ResourceTitleDelegate.hpp"
#include "model.hpp"
#include "ResultsTable.hpp"

namespace {
    QString pathForType(ResourceType type) noexcept {
        switch (type) { // NOLINT (-Wswitch-default)
            case ResourceType::plainText: return QStringLiteral(":/icons/type-text.svg");
            case ResourceType::cCppCode : return QStringLiteral(":/icons/type-cpp.svg");
            case ResourceType::htmlDoc  : return QStringLiteral(":/icons/type-html.svg");
            case ResourceType::pdfDoc   : return QStringLiteral(":/icons/type-pdf.svg");
            case ResourceType::epubDoc  : return QStringLiteral(":/icons/type-epub.svg");
            case ResourceType::unknown  : break;
        }
        return {};
    }
} // namespace

void ResourceTitleDelegate::paint(QPainter* painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const {
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    opt.text.clear();

    const QWidget* widget = option.widget;
    QStyle* style = (widget != nullptr) ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    // ==== LẤY DỮ LIỆU ====
    const QString text = index.data(Qt::DisplayRole).toString();
    const auto type = static_cast<ResourceType>(
        index.data(static_cast<int>(ResultsTable::ItemRole::resourceType)).toInt());

    const int targetHeight{20};
    int targetWidth = targetHeight;

    QSvgRenderer* renderer = getRenderer(type);
    if (renderer != nullptr) {
        QSize defaultSize = renderer->defaultSize();
        // Tính Width dựa trên Aspect Ratio: W_new = H_new * (W_orig / H_orig)
        if (defaultSize.height() > 0) {
            targetWidth = targetHeight * defaultSize.width() / defaultSize.height();
        }
    }

    constexpr int spacing{8};
    QRect rect = option.rect;

    QRect svgRect{rect.right() - targetWidth - spacing, rect.center().y() - (targetHeight / 2),
                  targetWidth, targetHeight};

    QRect textRect = rect.adjusted(spacing, 0, -(targetWidth + (spacing * 2)), 0);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(opt.palette.color(
        ((opt.state & QStyle::State_Selected) != 0) ? QPalette::HighlightedText : QPalette::Text));
    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);

    if (renderer != nullptr) { renderer->render(painter, svgRect); }

    painter->restore();
}

QSize ResourceTitleDelegate::sizeHint(const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const {
    QSize sz = QStyledItemDelegate::sizeHint(option, index);
    sz.setHeight(qMax(sz.height(), 30)); // NOLINT(readability-magic-numbers) // icon + padding
    return sz;
}

QSvgRenderer* ResourceTitleDelegate::getRenderer(ResourceType type) const {
    auto it = m_rendererCache.find(type);
    if (it != m_rendererCache.end()) { return it->second.get(); }

    QString path = pathForType(type);
    auto renderer = std::make_unique<QSvgRenderer>(path);

    if (!renderer->isValid()) { return nullptr; }

    QSvgRenderer* ptr = renderer.get();
    m_rendererCache[type] = std::move(renderer);

    return ptr;
}
