#include "gui/Viewer/ResourceViewerDialog.hpp"

#include "gui/Viewer/IResourceViewer.hpp"
#include "helper/DialogUtils.hpp"
#include "helper/SettingsManager.hpp"

#include <QCloseEvent>
#include <QDialog>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QMessageBox>
#include <QObject>
#include <QPixmap>
#include <QSizePolicy>
#include <QString>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtAssert>
#include <QtGlobal>
#include <algorithm>
#include <memory>
#include <utility>

ResourceViewerDialog::ResourceViewerDialog(QString const& title,
                                           std::unique_ptr<IResourceViewer> viewer,
                                           QWidget* parent)
    : QDialog(parent), m_viewer(std::move(viewer)) {
    Q_ASSERT(m_viewer);

    setAttribute(Qt::WA_DeleteOnClose);
    setupUi(title);
    setupActions();

    // adjustSize();                      // settle layout + toolbar
    setMinimumSize(minimumSizeHint()); // CỰC KỲ QUAN TRỌNG
}

void ResourceViewerDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);

    if (m_geometryRestored) {
        return;
    }

    // adjustSize();
    restoreGeometryLogic();
    m_geometryRestored = true;
}

void ResourceViewerDialog::closeEvent(QCloseEvent* event) {
    auto& qSettings = SettingsManager::instance();

    qSettings.set("window/dialog_viewer_posX", x());
    qSettings.set("window/dialog_viewer_posY", y());
    qSettings.set("window/dialog_viewer_width", width());
    qSettings.set("window/dialog_viewer_height", height());

    if (m_viewer) {
        if (!m_viewer->onClose(this)) {
            event->ignore();
            return;
        }
    }

    QDialog::closeEvent(event);
}

void ResourceViewerDialog::setupUi(QString const& title) {
    setWindowTitle(QString(tr("View detail resource: %1")).arg(title));

    auto* layout = new QVBoxLayout(this);

    setLayout(layout);

    if (m_viewer) {
        layout->addWidget(m_viewer->widget());
    }
}

void ResourceViewerDialog::setupActions() {
    if (!m_viewer) {
        return;
    }

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
    auto& qSettings = SettingsManager::instance();

    int const savedX = qSettings.get("window/dialog_viewer_posX", -1).toInt();
    int const savedY = qSettings.get("window/dialog_viewer_posY", -1).toInt();
    int const savedW = qSettings.get("window/dialog_viewer_width", -1).toInt();
    int const savedH = qSettings.get("window/dialog_viewer_height", -1).toInt();

    static constexpr QSize kDefaultSize{800, 800};

    QSize finalSize = kDefaultSize;
    if (savedW > 0 && savedH > 0) {
        finalSize.setWidth(std::max(savedW, kDefaultSize.width()));
        finalSize.setHeight(std::max(savedH, kDefaultSize.height()));
    }

    resize(finalSize);

    QScreen* scr = screen();
    if (scr == nullptr) {
        scr = QGuiApplication::primaryScreen();
    }
    Q_ASSERT(scr);

    QRect const screenGeom = scr->availableGeometry();

    QRect dlgRect = frameGeometry();

    if (savedX >= 0 && savedY >= 0) {
        dlgRect.moveTopLeft({savedX, savedY});
        dlgRect = ensureOnScreen(dlgRect);
        move(dlgRect.topLeft());
        return;
    }

    constexpr DialogAnchor kDefaultFallbackAnchor = DialogAnchor::Center;
    if (QWidget* parent = parentWidget()) {
        dlgRect = calcFallbackRect(parent, kDefaultFallbackAnchor);
        move(dlgRect.topLeft());
        return;
    }

    dlgRect.moveCenter(screenGeom.center());
    move(dlgRect.topLeft());
}

QRect ResourceViewerDialog::ensureOnScreen(QRect const& rect) const {
    QScreen* scr = screen();
    if (scr == nullptr) {
        scr = QGuiApplication::primaryScreen();
    }
    Q_ASSERT(scr);

    QRect const screenGeom = scr->availableGeometry();

    if (screenGeom.intersects(rect)) {
        return rect;
    }

    QRect fixed = rect;
    fixed.moveCenter(screenGeom.center());
    return fixed;
}

QRect ResourceViewerDialog::calcFallbackRect(QWidget* parent, DialogAnchor anchor) const {
    Q_ASSERT(parent);

    QRect const parentFrame = parent->frameGeometry();
    QSize const dialogFrameSize = frameGeometry().size();

    QRect r(QPoint{0, 0}, dialogFrameSize);

    switch (anchor) {
        case DialogAnchor::Left  : r.moveTopRight(parentFrame.topLeft()); break;
        case DialogAnchor::Right : r.moveTopLeft(parentFrame.topRight()); break;
        case DialogAnchor::Top   : r.moveBottomLeft(parentFrame.topLeft()); break;
        case DialogAnchor::Bottom: r.moveTopLeft(parentFrame.bottomLeft()); break;
        case DialogAnchor::Center: r.moveCenter(parentFrame.center()); break;
    }

    return ensureOnScreen(r);
}
