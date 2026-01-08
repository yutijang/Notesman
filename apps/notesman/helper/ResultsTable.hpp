#pragma once

#include <QTableWidget>
#include <Qt>
#include <QObject>

class QWidget;
class QMouseEvent;
class QFocusEvent;
class QShowEvent;
class QEvent;
class QResizeEvent;

// Prevent highlight cell last clicked when miss focus
class ResultsTable final : public QTableWidget {
        Q_OBJECT

    public:
        explicit ResultsTable(QWidget* parent = nullptr);

        // ItemRole base type must be int to match Qt ItemDataRole API
        // NOLINTNEXTLINE(performance-enum-size)
        enum class ItemRole : int {
            resourceId = Qt::UserRole + 1,
            resourceType,  // Kiểu tài nguyên
            searchSnippet, // Chứa đoạn trích từ nội dung (từ hàm snippet() của FTS5)
            fullPath,      // Đường dẫn file đầy đủ (để hiển thị nếu không có snippet)
            tagList,       // Danh sách tag dưới dạng chuỗi (vd: "#cpp #stl")
            matchType      // Để Delegate biết lý do khớp (Title, Tag, hay Content)
        };

    protected:
        void mousePressEvent(QMouseEvent* event) override;
        void focusOutEvent(QFocusEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void changeEvent(QEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:
        void updateLastHeaderBorder();
};
