#pragma once

#include <QObject>
#include <QToolButton>

class QWidget;
class QMouseEvent;

class InfoCornerWidget final : public QToolButton {
        Q_OBJECT

    public:
        explicit InfoCornerWidget(QWidget* parent = nullptr);

        void retranslateUi();

    Q_SIGNALS:
        void checkUpdateRequested();
        void aboutRequested();

    protected:
        void mousePressEvent(QMouseEvent* event) override;

    private:
        void onCheckUpdate();
        void onAbout();
};
