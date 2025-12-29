#pragma once

#include <QToolButton>
#include <QObject>

class QWidget;
class QMouseEvent;

class InfoCornerWidget final : public QToolButton {
        Q_OBJECT

    public:
        explicit InfoCornerWidget(QWidget* parent = nullptr);

        void retranslateUi();

    protected:
        void mousePressEvent(QMouseEvent* event) override;

    signals:
        void checkUpdateRequested();
        void aboutRequested();

    private slots:
        void onCheckUpdate();
        void onAbout();
};
