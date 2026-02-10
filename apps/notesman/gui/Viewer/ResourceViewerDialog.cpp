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

    // adjustSize();                      // settle layout + toolbar
    setMinimumSize(minimumSizeHint()); // CỰC KỲ QUAN TRỌNG
}

void ResourceViewerDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);

    if (m_geometryRestored) { return; }

    // adjustSize();
    restoreGeometryLogic();
    m_geometryRestored = true;
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

    const int savedX = settings.get("window/dialog_viewer_posX", -1).toInt();
    const int savedY = settings.get("window/dialog_viewer_posY", -1).toInt();
    const int savedW = settings.get("window/dialog_viewer_width", -1).toInt();
    const int savedH = settings.get("window/dialog_viewer_height", -1).toInt();

    static constexpr QSize kDefaultSize{800, 800};

    QSize finalSize = kDefaultSize;
    if (savedW > 0 && savedH > 0) {
        finalSize.setWidth(std::max(savedW, kDefaultSize.width()));
        finalSize.setHeight(std::max(savedH, kDefaultSize.height()));
    }

    resize(finalSize);

    QScreen* scr = screen();
    if (scr == nullptr) { scr = QGuiApplication::primaryScreen(); }
    Q_ASSERT(scr);

    const QRect screenGeom = scr->availableGeometry();

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

QRect ResourceViewerDialog::ensureOnScreen(const QRect &rect) const {
    QScreen* scr = screen();
    if (scr == nullptr) { scr = QGuiApplication::primaryScreen(); }
    Q_ASSERT(scr);

    const QRect screenGeom = scr->availableGeometry();

    if (screenGeom.intersects(rect)) { return rect; }

    QRect fixed = rect;
    fixed.moveCenter(screenGeom.center());
    return fixed;
}

QRect ResourceViewerDialog::calcFallbackRect(QWidget* parent, DialogAnchor anchor) const {
    Q_ASSERT(parent);

    const QRect parentFrame = parent->frameGeometry();
    const QSize dialogFrameSize = frameGeometry().size();

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
