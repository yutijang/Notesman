#include <QPainter>
#include <QApplication>

#include "ResourceTitleDelegate.hpp"
#include "model.hpp"
#include "ResultsTable.hpp"

namespace {
    QIcon iconForType(ResourceType type) noexcept {
        switch (type) { // NOLINT (-Wswitch-default)
            case ResourceType::plainText: return QIcon(":/icons/type-text.ico");
            case ResourceType::cCppCode : return QIcon(":/icons/type-cpp.ico");
            case ResourceType::htmlDoc  : return QIcon(":/icons/type-html.ico");
            case ResourceType::pdfDoc   : return QIcon(":/icons/type-pdf.ico");
            case ResourceType::epubDoc  : return QIcon(":/icons/type-epub.ico");
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

    const QIcon icon = iconForType(type);

    constexpr int iconSize{48};
    constexpr int spacing{6};

    QRect rect = option.rect;

    QRect iconRect{rect.right() - iconSize - spacing, rect.center().y() - (iconSize / 2), iconSize,
                   iconSize};

    QRect textRect = rect.adjusted(spacing, 0, -(iconSize + (spacing * 2)), 0);

    painter->setPen(opt.palette.color(
        ((opt.state & QStyle::State_Selected) != 0) ? QPalette::HighlightedText : QPalette::Text));

    painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);

    icon.paint(painter, iconRect, Qt::AlignCenter);
}

QSize ResourceTitleDelegate::sizeHint(const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const {
    QSize sz = QStyledItemDelegate::sizeHint(option, index);
    sz.setHeight(qMax(sz.height(), 22)); // NOLINT(readability-magic-numbers) // icon + padding
    return sz;
}
