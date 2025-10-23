#include <QObject>
#include <QTextCharFormat>
#include <QFocusEvent>
#include <QTextEdit>
#include <QColor>
#include <QEvent>
#include <QTextCursor>
#include "CodeEditorLineHighlighter.hpp"

CodeEditorLineHighlighter::CodeEditorLineHighlighter(QTextEdit* editor)
    : QObject(editor), m_editor(editor) {
    Q_ASSERT(editor);

    // Kết nối tín hiệu di chuyển con trỏ
    connect(m_editor, &QTextEdit::cursorPositionChanged, this,
            &CodeEditorLineHighlighter::highlightCurrentLine);

    // Cài đặt bộ lọc sự kiện để bắt Focus In/Out
    m_editor->installEventFilter(this);

    // Thiết lập trạng thái ban đầu: nếu đã có focus thì highlight, ngược lại thì không
    updateHighlighting(m_editor->hasFocus());
}

void CodeEditorLineHighlighter::setColors(const QColor &background, const QColor &blurBackground) {
    m_background = background;
    m_blurBackground = blurBackground;
    // Cập nhật ngay lập tức
    updateHighlighting(m_editor->hasFocus());
}

// Bộ lọc sự kiện để bắt FocusIn và FocusOut
bool CodeEditorLineHighlighter::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_editor) {
        if (event->type() == QEvent::FocusIn) {
            updateHighlighting(true);
        } else if (event->type() == QEvent::FocusOut) {
            updateHighlighting(false);
        }
    }
    // Quan trọng: Luôn gọi hàm eventFilter của lớp cơ sở
    return QObject::eventFilter(obj, event);
}

// Phương thức logic chính để BẬT/TẮT highlight dựa trên trạng thái focus
void CodeEditorLineHighlighter::updateHighlighting(bool hasFocus) {
    if (hasFocus) {
        // Nếu có focus, gọi highlightCurrentLine để highlight với màu nền chuẩn
        highlightCurrentLine();
    } else {
        // Nếu mất focus, áp dụng highlight với màu nền mờ
        highlightCurrentLine();

        // HOẶC: Nếu muốn tắt hoàn toàn khi mất focus:
        // m_editor->setExtraSelections({});
    }
}

// SLOT: Áp dụng highlight cho dòng hiện tại
void CodeEditorLineHighlighter::highlightCurrentLine() {
    if (m_editor == nullptr) { return; }

    QList<QTextEdit::ExtraSelection> extras;

    // Chỉ tạo highlight khi tài liệu không rỗng HOẶC khi có focus
    if (m_editor->document()->isEmpty() && !m_editor->hasFocus()) {
        m_editor->setExtraSelections(extras); // Tắt highlight
        return;
    }

    QTextEdit::ExtraSelection sel;
    QTextCursor cursor = m_editor->textCursor();
    sel.cursor = cursor;
    sel.cursor.clearSelection(); // Đảm bảo chỉ highlight dòng, không phải text đang chọn

    QTextCharFormat fmt;

    // Chọn màu nền dựa trên trạng thái focus
    QColor currentBg = m_editor->hasFocus() ? m_background : m_blurBackground;

    fmt.setBackground(currentBg);
    // Đây là Best Practice để highlight toàn bộ chiều rộng viewport
    fmt.setProperty(QTextFormat::FullWidthSelection, true);

    sel.format = fmt;
    extras.append(sel);

    // Áp dụng list ExtraSelections (chỉ có 1 dòng highlight)
    m_editor->setExtraSelections(extras);
}