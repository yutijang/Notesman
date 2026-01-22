#include <algorithm>
#include <memory>
#include <utility>
#include <QCloseEvent>
#include <QMessageBox>
#include <QString>
#include <QWidget>
#include <QDialog>
#include <QToolBar>
#include <QTimer>
#include <QStyle>
#include <QVBoxLayout>
#include <QIcon>
#include <QPixmap>
#include <Qt>
#include <QFont>
#include <QSizePolicy>
#include <QObject>
#include <QtGlobal>
#include <QtAssert>
#include <QGuiApplication>

#include "ResourceViewerDialog.hpp"
#include "DialogUtils.hpp"
#include "IResourceViewer.hpp"
#include "SettingsManager.hpp"

ResourceViewerDialog::ResourceViewerDialog(const QString &title,
                                           std::unique_ptr<IResourceViewer> viewer, QWidget* parent)
    : QDialog(parent), m_viewer(std::move(viewer)) {
    Q_ASSERT(m_viewer);

    setAttribute(Qt::WA_DeleteOnClose);
    setupUi(title);
    setupActions();
}

void ResourceViewerDialog::showEvent(QShowEvent* event) {
    if (!m_geometryRestored) {
        restoreGeometryLogic();
        m_geometryRestored = true;
    }

    QDialog::showEvent(event);
}

void ResourceViewerDialog::closeEvent(QCloseEvent* event) {
    auto &settings = SettingsManager::instance();

    settings.set("window/dialog_viewer_posX", x());
    settings.set("window/dialog_viewer_posY", y());
    settings.set("window/dialog_viewer_width", width());
    settings.set("window/dialog_viewer_height", height());

    if (m_viewer) {
        if (!m_viewer->onClose(this)) {
            event->ignore();
            return;
        }
    }

    QDialog::closeEvent(event);
}

void ResourceViewerDialog::setupUi(const QString &title) {
    setWindowTitle(QString(tr("View detail resource: %1")).arg(title));

    auto* layout = new QVBoxLayout(this);

    setLayout(layout);

    if (m_viewer) { layout->addWidget(m_viewer->widget()); }
}

void ResourceViewerDialog::setupActions() {
    if (!m_viewer) { return; }

    auto* toolbar = new QToolBar(this);
    toolbar->setObjectName("ResourceViewerToolbar");
    toolbar->setMovable(false);

    // Spacer trái
    {
        auto* spacer = new QWidget(toolbar);
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        toolbar->addWidget(spacer);
    }

    // === Ủy quyền cho viewer tự thêm action ===
    m_viewer->setupToolbar(toolbar);

    // Spacer phải
    {
        auto* spacer = new QWidget(toolbar);
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        toolbar->addWidget(spacer);
    }

    layout()->setMenuBar(toolbar);
}

void ResourceViewerDialog::restoreGeometryLogic() {
    auto &settings = SettingsManager::instance();

    const int x = settings.get("window/dialog_viewer_posX", -1).toInt();
    const int y = settings.get("window/dialog_viewer_posY", -1).toInt();
    const int w = settings.get("window/dialog_viewer_width", -1).toInt();
    const int h = settings.get("window/dialog_viewer_height", -1).toInt();

    static constexpr QSize kDefaultSize{800, 800};

    QSize targetSize = kDefaultSize;

    if (w > 0 && h > 0) {
        targetSize.setWidth(std::max(w, kDefaultSize.width()));
        targetSize.setHeight(std::max(h, kDefaultSize.height()));
    }

    resize(targetSize);

    QScreen* scr = screen();
    if (scr == nullptr) { scr = QGuiApplication::primaryScreen(); }

    const QRect screenGeom = scr->availableGeometry();

    QRect dlgRect = frameGeometry();

    if (x >= 0 && y >= 0) {
        dlgRect.moveTopLeft({x, y});
    } else {
        dlgRect.moveCenter(screenGeom.center());
    }

    if (!screenGeom.intersects(dlgRect)) { dlgRect.moveCenter(screenGeom.center()); }

    move(dlgRect.topLeft());
}
