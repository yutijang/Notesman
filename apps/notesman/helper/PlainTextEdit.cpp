#include "PlainTextEdit.hpp"

#include <QColor>
#include <QMimeData>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QRect>
#include <QString>
#include <QTextBlock>
#include <QTextEdit>
#include <QTextOption>
#include <QWidget>
#include <algorithm>
#include <cmath>

PlainTextEdit::PlainTextEdit(QWidget* parent)
    : QPlainTextEdit(parent), m_lineNumberArea(new LineNumberArea(this)) {
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    connect(
        this, &QPlainTextEdit::blockCountChanged, this, &PlainTextEdit::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &PlainTextEdit::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this]() {
        viewport()->update();
    });

    // setLineWrapMode(QTextEdit::WidgetWidth);

    updateLineNumberAreaWidth();
}

void PlainTextEdit::insertFromMimeData(QMimeData const* source) {
    if (source->hasText()) {
        insertPlainText(source->text());
    } else {
        QPlainTextEdit::insertFromMimeData(source);
    }
}

int PlainTextEdit::lineNumberAreaWidth() const {
    int const digits = std::max(2, static_cast<int>(std::log10(blockCount() + 1)) + 1);

    // VSCode-like padding
    int const paddingLeft = 6;
    int const paddingRight = 8;

    QFontMetrics metrics(font());
    int const numberWidth = metrics.horizontalAdvance(QString(digits, QChar('9')));

    return paddingLeft + numberWidth + paddingRight;
}

void PlainTextEdit::updateLineNumberAreaWidth() {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void PlainTextEdit::updateLineNumberArea(QRect const& rect, int dy) {
    if (dy != 0) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth();
    }
}

void PlainTextEdit::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);

    QRect const cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void PlainTextEdit::lineNumberAreaPaintEvent(QPaintEvent* event) {
    QPainter painter(m_lineNumberArea);

    // Detect theme via palette
    QColor const baseColor = palette().color(QPalette::Window);
    bool const isDark = baseColor.value() < 128;

    // NOLINTNEXTLINE(readability-magic-numbers)
    QColor bg = isDark ? baseColor.darker(115) : baseColor.lighter(110);
    QColor fg = isDark ? QColor("#858585") : QColor("#237893");

    painter.fillRect(event->rect(), bg);
    painter.setPen(fg);

    int const rightPadding = 8;

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();

    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    QFontMetrics fm(font());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString const number = QString::number(blockNumber + 1);

            int const x = lineNumberAreaWidth() - fm.horizontalAdvance(number) - rightPadding;
            int const y = top + fm.ascent() + 1;

            painter.drawText(x, y, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}
