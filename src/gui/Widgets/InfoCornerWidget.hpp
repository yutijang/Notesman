#pragma once

#include <QToolButton>

class QWidget;
class QMouseEvent;

class InfoCornerWidget final : public QToolButton {
        Q_OBJECT

    public:
        explicit InfoCornerWidget(QWidget* parent = nullptr);

    protected:
        void mousePressEvent(QMouseEvent* event) override;

    signals:
        void checkUpdateRequested();
        void aboutRequested();

    private slots:
        void onCheckUpdate();
        void onAbout();
};
