#pragma once

#include <QTableWidget>

class QMouseEvent;
class QFocusEvent;
class QShowEvent;

// Prevent highlight cell last clicked when miss focus
class ResultsTable : public QTableWidget {
        Q_OBJECT
    public:
        explicit ResultsTable(QWidget* parent = nullptr);

    protected:
        void mousePressEvent(QMouseEvent* event) override;
        void focusOutEvent(QFocusEvent* event) override;
        void showEvent(QShowEvent* event) override;
};
