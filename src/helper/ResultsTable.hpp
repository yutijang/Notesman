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
        enum class ItemRole : int { resourceId = Qt::UserRole + 1, resourceType };

    protected:
        void mousePressEvent(QMouseEvent* event) override;
        void focusOutEvent(QFocusEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void changeEvent(QEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:
        void updateLastHeaderBorder();
};
