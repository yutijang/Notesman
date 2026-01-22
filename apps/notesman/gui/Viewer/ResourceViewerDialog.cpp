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
    auto &settings = SettingsManager::instance();

    // Đọc vị trí lưu
    const int x = settings.get("window/dialog_viewer_posX", -1).toInt();
    const int y = settings.get("window/dialog_viewer_posY", -1).toInt();
    const int w = settings.get("window/dialog_viewer_width", -1).toInt();
    const int h = settings.get("window/dialog_viewer_height", -1).toInt();

    static constexpr int dialogWidth{640};
    const int mainH{800};
    const int frameH = this->style()->pixelMetric(QStyle::PM_TitleBarHeight) +
                       (this->style()->pixelMetric(QStyle::PM_DefaultFrameWidth) * 2);
    const int dialogHeight = mainH - frameH + 30;

    if (x != -1 && y != -1) {
        this->move(x, y);
    } else {
        const QScreen* screen = this->screen();
        const QRect geom = screen->availableGeometry();
        this->move(geom.center() - frameGeometry().center());
    }

    if (w >= dialogWidth && h >= dialogHeight) {
        this->resize(w, h);
    } else {
        this->resize(dialogWidth + 30, dialogHeight);
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
