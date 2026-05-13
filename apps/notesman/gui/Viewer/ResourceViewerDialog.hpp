#pragma once

#include "IResourceViewer.hpp"

#include <QDialog>
#include <QObject>
#include <QString>
#include <cstdint>
#include <memory>

class QWidget;
class QShowEvent;
class QCloseEvent;

class ResourceViewerDialog final : public QDialog {
    Q_OBJECT

  public:
    explicit ResourceViewerDialog(QString const& title,
                                  std::unique_ptr<IResourceViewer> viewer,
                                  QWidget* parent = nullptr);
    ~ResourceViewerDialog() override = default;

  protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

  private:
    void setupUi(QString const& title);
    void setupActions();
    void restoreGeometryLogic();

    enum class DialogAnchor : std::uint8_t { Center, Left, Right, Top, Bottom };

    [[nodiscard]] QRect ensureOnScreen(QRect const& rect) const;
    QRect calcFallbackRect(QWidget* parent, DialogAnchor anchor) const;

    bool m_geometryRestored{};
    std::unique_ptr<IResourceViewer> m_viewer;
};
