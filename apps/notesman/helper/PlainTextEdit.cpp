#include <algorithm>
#include <cmath>
#include <QMimeData>
#include <QWidget>
#include <QTextEdit>
#include <QPainter>
#include <QTextBlock>
#include <QPlainTextEdit>
#include <QTextOption>
#include <QString>
#include <QRect>
#include <QPalette>
#include <QColor>

#include "PlainTextEdit.hpp"

PlainTextEdit::PlainTextEdit(QWidget* parent)
    : QPlainTextEdit(parent), m_lineNumberArea(new LineNumberArea(this)) {
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    connect(this, &QPlainTextEdit::blockCountChanged, this,
            &PlainTextEdit::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &PlainTextEdit::updateLineNumberArea);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this]() { viewport()->update(); });

    // setLineWrapMode(QTextEdit::WidgetWidth);

    updateLineNumberAreaWidth();
}

void PlainTextEdit::insertFromMimeData(const QMimeData* source) {
    if (source->hasText()) {
        insertPlainText(source->text());
    } else {
        QPlainTextEdit::insertFromMimeData(source);
    }
}

int PlainTextEdit::lineNumberAreaWidth() const {
    const int digits = std::max(2, static_cast<int>(std::log10(blockCount() + 1)) + 1);

    // VSCode-like padding
    const int paddingLeft = 6;
    const int paddingRight = 8;

    QFontMetrics metrics(font());
    const int numberWidth = metrics.horizontalAdvance(QString(digits, QChar('9')));

    return paddingLeft + numberWidth + paddingRight;
}

void PlainTextEdit::updateLineNumberAreaWidth() {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void PlainTextEdit::updateLineNumberArea(const QRect &rect, int dy) {
    if (dy != 0) {
        m_lineNumberArea->scroll(0, dy);
    } else {
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) { updateLineNumberAreaWidth(); }
}

void PlainTextEdit::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);

    const QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void PlainTextEdit::lineNumberAreaPaintEvent(QPaintEvent* event) {
    QPainter painter(m_lineNumberArea);

    // Detect theme via palette
    const QColor baseColor = palette().color(QPalette::Window);
    const bool isDark = baseColor.value() < 128;

    // NOLINTNEXTLINE(readability-magic-numbers)
    QColor bg = isDark ? baseColor.darker(115) : baseColor.lighter(110);
    QColor fg = isDark ? QColor("#858585") : QColor("#237893");

    painter.fillRect(event->rect(), bg);
    painter.setPen(fg);

    const int rightPadding = 8;

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();

    int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + static_cast<int>(blockBoundingRect(block).height());

    QFontMetrics fm(font());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            const QString number = QString::number(blockNumber + 1);

            const int x = lineNumberAreaWidth() - fm.horizontalAdvance(number) - rightPadding;
            const int y = top + fm.ascent() + 1;

            painter.drawText(x, y, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++blockNumber;
    }
}
