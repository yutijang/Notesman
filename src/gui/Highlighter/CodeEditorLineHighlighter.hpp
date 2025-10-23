// File: src/gui/Highlighter/CodeEditorLineHighlighter.hpp
#pragma once

class QObject;
class QTextEdit;
class QEvent;
class QColor;

class CodeEditorLineHighlighter final : public QObject {
        Q_OBJECT

    public:
        explicit CodeEditorLineHighlighter(QTextEdit* editor);

        // Đặt màu nền khi có focus và mất focus
        void setColors(const QColor &background, const QColor &blurBackground);

    protected:
        // Ghi đè eventFilter để bắt sự kiện FocusIn và FocusOut của QTextEdit
        bool eventFilter(QObject* obj, QEvent* event) override;

    private slots:
        void highlightCurrentLine();             // Cập nhật highlight khi con trỏ thay đổi

    private:                                     // NOLINT(readability-redundant-access-specifiers)
        QTextEdit* m_editor;
        QColor m_background = QColor("#dBdBdB"); // Màu nền dòng khi CÓ focus
        QColor m_blurBackground = QColor("#efefef"); // Màu nền dòng khi MẤT focus

        // Phương thức xử lý thay đổi focus và bật/tắt highlight
        void updateHighlighting(bool hasFocus);
};
