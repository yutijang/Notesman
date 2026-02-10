#pragma once

#include <cstdint>
#include <memory>
#include <QDialog>
#include <QString>
#include <QObject>

#include "IResourceViewer.hpp"

class QWidget;
class QShowEvent;
class QCloseEvent;

class ResourceViewerDialog final : public QDialog {
        Q_OBJECT

    public:
        explicit ResourceViewerDialog(const QString &title, std::unique_ptr<IResourceViewer> viewer,
                                      QWidget* parent = nullptr);
        ~ResourceViewerDialog() override = default;

    protected:
        void showEvent(QShowEvent* event) override;
        void closeEvent(QCloseEvent* event) override;

    private:
        void setupUi(const QString &title);
        void setupActions();
        void restoreGeometryLogic();

        enum class DialogAnchor : std::uint8_t { Center, Left, Right, Top, Bottom };

        [[nodiscard]] QRect ensureOnScreen(const QRect &rect) const;
        QRect calcFallbackRect(QWidget* parent, DialogAnchor anchor) const;

        bool m_geometryRestored{};
        std::unique_ptr<IResourceViewer> m_viewer;
};
