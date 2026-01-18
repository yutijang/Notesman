#pragma once

#include <memory>
#include <QDialog>
#include <QString>
#include <QObject>

#include "IResourceViewer.hpp"

class QWidget;
class QCloseEvent;

class ResourceViewerDialog final : public QDialog {
        Q_OBJECT

    public:
        explicit ResourceViewerDialog(const QString &title, std::unique_ptr<IResourceViewer> viewer,
                                      QWidget* parent = nullptr);
        ~ResourceViewerDialog() override = default;

    protected:
        void closeEvent(QCloseEvent* event) override;

    private:
        void setupUi(const QString &title);
        void setupActions();

        std::unique_ptr<IResourceViewer> m_viewer;
};
