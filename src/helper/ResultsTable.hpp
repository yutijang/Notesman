#pragma once

#include <QTableWidget>

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

    protected:
        void mousePressEvent(QMouseEvent* event) override;
        void focusOutEvent(QFocusEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void changeEvent(QEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;

    private:
        void updateLastHeaderBorder();
};
