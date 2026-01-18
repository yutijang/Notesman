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

#include "ResourceViewerDialog.hpp"
#include "DialogUtils.hpp"
#include "IResourceViewer.hpp"

ResourceViewerDialog::ResourceViewerDialog(const QString &title,
                                           std::unique_ptr<IResourceViewer> viewer, QWidget* parent)
    : QDialog(parent), m_viewer(std::move(viewer)) {
    Q_ASSERT(m_viewer);

    setAttribute(Qt::WA_DeleteOnClose);
    setupUi(title);
    setupActions();
}

void ResourceViewerDialog::closeEvent(QCloseEvent* event) {
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
    static constexpr int dialogWidth{640};
    static constexpr int offset{30};
    const int mainH{800};
    this->setMinimumWidth(dialogWidth + offset);
    const int frameH = this->style()->pixelMetric(QStyle::PM_TitleBarHeight) +
                       (this->style()->pixelMetric(QStyle::PM_DefaultFrameWidth) * 2);
    this->resize(dialogWidth, mainH - frameH + offset);

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
