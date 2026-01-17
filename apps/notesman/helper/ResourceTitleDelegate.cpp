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
#include <QFont>
#include <QColor>
#include <memory>
#include <utility>
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
            case ResourceType::unknown  :
            case ResourceType::count    : break;
        }
        return {};
    }
} // namespace

void ResourceTitleDelegate::paint(QPainter* painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const {
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    // Không để style vẽ text mặc định
    opt.text.clear();

    const QWidget* widget = option.widget;
    QStyle* style = (widget != nullptr) ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    // ===== LẤY DỮ LIỆU =====
    const QString titleText = index.data(Qt::DisplayRole).toString();
    const QString subText =
        index.data(static_cast<int>(ResultsTable::ItemRole::displaySubText)).toString();
    const auto type = static_cast<ResourceType>(
        index.data(static_cast<int>(ResultsTable::ItemRole::resourceType)).toInt());
    const auto flags = static_cast<ResourceFlags>(
        index.data(static_cast<int>(ResultsTable::ItemRole::resourceFlags)).toInt());

    // ===== ICON SVG =====
    constexpr int iconSize = 20;
    constexpr int spacing = 8;

    int iconWidth = iconSize;
    QSvgRenderer* renderer = getRenderer(type);
    if ((renderer != nullptr) && renderer->defaultSize().height() > 0) {
        iconWidth = iconSize * renderer->defaultSize().width() / renderer->defaultSize().height();
    }

    QRect rect = option.rect;

    QRect iconRect(rect.right() - iconWidth - spacing, rect.center().y() - (iconSize / 2),
                   iconWidth, iconSize);

    QRect textRect = rect.adjusted(spacing, 0, -(iconWidth + (spacing * 2)), 0);

    // ===== FONT =====
    QFont titleFont = opt.font;
    titleFont.setBold(true);

    QFont subFont = opt.font;
    subFont.setPointSizeF(opt.font.pointSizeF() - 1);

    QFontMetrics titleFm(titleFont);
    QFontMetrics subFm(subFont);

    // ===== LAYOUT DỌC =====
    constexpr int paddingTop = 2;
    // constexpr int paddingBottom = 6;
    constexpr int lineSpacing = 2;

    int y = textRect.top() + paddingTop;

    QRect titleRect(textRect.left(), y, textRect.width(), titleFm.height());

    y += titleFm.height() + lineSpacing;

    QRect subRect(textRect.left(), y, textRect.width(), subFm.height());

    painter->save();
    painter->setRenderHint(QPainter::TextAntialiasing);

    // ===== TITLE =====
    painter->setFont(titleFont);
    painter->setPen(opt.palette.color(
        ((opt.state & QStyle::State_Selected) != 0) ? QPalette::HighlightedText : QPalette::Text));

    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignTop, titleText);

    // ===== SUB TEXT =====
    if (!subText.isEmpty()) {
        QString subTextForDisplay = tr("Latest modified: %1").arg(subText);

        if (hasFlag(flags, ResourceFlags::matchContent)) {
            subTextForDisplay = subText;
        } else if (hasFlag(flags, ResourceFlags::matchTag)) {
            subTextForDisplay = tr("Tags: %1").arg(subText);
        }

        painter->setFont(subFont);

        QColor subColor = opt.palette.color(QPalette::Text);
        subColor.setAlpha(160); // NOLINT(readability-magic-numbers)

        painter->setPen(((opt.state & QStyle::State_Selected) != 0)
                            ? opt.palette.color(QPalette::HighlightedText)
                            : subColor);

        painter->drawText(subRect, Qt::AlignLeft | Qt::AlignTop, subTextForDisplay);
    }

    // ===== ICON =====
    if (renderer != nullptr) { renderer->render(painter, iconRect); }

    painter->restore();
}

QSize ResourceTitleDelegate::sizeHint(const QStyleOptionViewItem &option,
                                      const QModelIndex &index) const {
    QSize sz = QStyledItemDelegate::sizeHint(option, index);
    sz.setHeight(qMax(sz.height(), 48)); // NOLINT(readability-magic-numbers) // icon + padding
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
