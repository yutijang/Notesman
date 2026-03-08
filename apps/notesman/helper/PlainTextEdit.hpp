#pragma once

#include <QObject>
#include <QPlainTextEdit>
#include <QRect>
#include <QWidget>

class QMimeData;
class QPaintEvent;
class QResizeEvent;
class LineNumberArea;

class PlainTextEdit final : public QPlainTextEdit {
        Q_OBJECT

    public:
        explicit PlainTextEdit(QWidget* parent = nullptr);

        [[nodiscard]] int lineNumberAreaWidth() const;
        void lineNumberAreaPaintEvent(QPaintEvent* event);

    protected:
        void resizeEvent(QResizeEvent* event) override;
        void insertFromMimeData(QMimeData const* source) override;

    private:
        void updateLineNumberAreaWidth();
        void updateLineNumberArea(QRect const& rect, int dy);

        QWidget* m_lineNumberArea;
};

class LineNumberArea final : public QWidget {
        Q_OBJECT

    public:
        explicit LineNumberArea(PlainTextEdit* editor) : QWidget(editor), m_editor(editor) {}

        [[nodiscard]] QSize sizeHint() const override {
            return {m_editor->lineNumberAreaWidth(), 0};
        }

    protected:
        void paintEvent(QPaintEvent* event) override { m_editor->lineNumberAreaPaintEvent(event); }

    private:
        PlainTextEdit* m_editor;
};
