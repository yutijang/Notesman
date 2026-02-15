#include <memory>
#include <algorithm>
#include <utility>
#include <poppler-document.h>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QTimer>
#include <QLayoutItem>
#include <QtConcurrent>
#include <QtConcurrentRun>
#include <QFutureWatcher>
#include <QRect>
#include <QObject>
#include <QtPreprocessorSupport>

#include "PdfViewer.hpp"
#include "PdfPageWidget.hpp"

PdfViewer::PdfViewer(QString pdfPath, QWidget* parent) : m_pdfPath(std::move(pdfPath)) {
    setupUi(parent);

    m_scrollDebounce.setSingleShot(true);
    m_scrollDebounce.setInterval(40); // NOLINT(readability-magic-numbers)

    QObject::connect(&m_scrollDebounce, &QTimer::timeout, m_rootWidget,
                     [this] { renderVisiblePages(); });

    QTimer::singleShot(0, m_rootWidget, [this] { loadDocument(); });
}

PdfViewer::~PdfViewer() {
    m_docWatcher.cancel();
    m_docWatcher.waitForFinished();
    m_document.reset();
}

QWidget* PdfViewer::widget() {
    return m_rootWidget;
}

bool PdfViewer::onClose(QWidget* /*parent*/) {
    return true;
}

void PdfViewer::setupUi(QWidget* parent) {
    m_rootWidget = new PdfViewerRootWidget(this, parent);

    m_scrollArea = new QScrollArea(m_rootWidget);
    m_scrollArea->setWidgetResizable(true);

    QObject::connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, m_rootWidget,
                     [this] { onScroll(); });

    m_pagesContainer = new QWidget;
    m_pagesLayout = new QVBoxLayout(m_pagesContainer);
    m_pagesLayout->setContentsMargins(8, 8, 8, 8); // NOLINT(readability-magic-numbers)
    m_pagesLayout->setSpacing(12);                 // NOLINT(readability-magic-numbers)

    m_scrollArea->setWidget(m_pagesContainer);

    auto* layout = new QVBoxLayout(m_rootWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_scrollArea);
}

void PdfViewer::setupToolbar(QToolBar* toolbar) {
    Q_UNUSED(toolbar)
}

void PdfViewer::loadDocument() {
    if (m_docWatcher.isRunning()) { return; }

    QObject::connect(&m_docWatcher, &QFutureWatcher<std::shared_ptr<poppler::document>>::finished,
                     m_rootWidget, [this]() {
                         auto doc = m_docWatcher.result();

                         if (!doc) {
                             QMessageBox::critical(m_rootWidget, QObject::tr("PDF error"),
                                                   QObject::tr("Cannot open PDF file."));
                             return;
                         }

                         m_document = doc; // shared ownership

                         createAndRenderFirstPageImmediately();
                         createPageWidgets();
                     });

    m_docWatcher.setFuture(QtConcurrent::run([path = m_pdfPath]() {
        return std::shared_ptr<poppler::document>(
            poppler::document::load_from_file(path.toStdString()));
    }));
}

void PdfViewer::clearPages() {
    while (auto* item = m_pagesLayout->takeAt(0)) {
        if (auto* w = item->widget()) { w->deleteLater(); }
        delete item;
    }
}

void PdfViewer::createPageWidgets() {
    m_pageCreateIndex = 1;
    QTimer::singleShot(0, m_rootWidget, [this] { createPageWidgetsStep(); });
}

void PdfViewer::onScroll() {
    if (!m_warmupDone) {
        m_scrollDebounce.setInterval(80); // NOLINT(readability-magic-numbers)
    } else {
        m_scrollDebounce.setInterval(40); // NOLINT(readability-magic-numbers)
    }

    m_lastScrollY = m_scrollArea->verticalScrollBar()->value();
    m_scrollDebounce.start();
}

void PdfViewer::renderVisiblePages() {
    if (m_rendering) { return; }
    m_rendering = true;

    auto* vp = m_scrollArea->viewport();
    int const scrollY = m_scrollArea->verticalScrollBar()->value();
    int const vh = vp->height();
    int const vw = vp->width();

    bool const scrollingDown = scrollY >= m_lastScrollY;
    m_lastScrollY = scrollY;

    // prefetch xa hơn về phía sắp cuộn tới
    QRect viewportRect(0, scrollY, vw, vh);
    QRect prefetchRect;

    if (scrollingDown) {
        prefetchRect = viewportRect.adjusted(0, -vh / 2, 0, vh * 3);
    } else {
        prefetchRect = viewportRect.adjusted(0, -vh * 3, 0, vh / 2);
    }

    constexpr int kMaxPagesPerPass = 4;
    int rendered{};

    double const screenDpi = std::min<double>(vp->logicalDpiX(), 144.0);
    bool const lowQualityPass = !m_warmupDone;
    int const count = m_pagesLayout->count();
    for (int i = 0; i < count && rendered < kMaxPagesPerPass; ++i) {
        auto* page = qobject_cast<PdfPageWidget*>(m_pagesLayout->itemAt(i)->widget());
        if (page == nullptr) { continue; }

        QRect const pageRect = page->geometry();

        if (!pageRect.intersects(prefetchRect)) {
            QRect const farRect = viewportRect.adjusted(0, -vh * 4, 0, vh * 4);
            if (!pageRect.intersects(farRect)) { page->releaseImage(); }
            continue;
        }

        double const dpi =
            (lowQualityPass ? 96.0 : screenDpi) *
            (static_cast<double>(page->width()) / static_cast<double>(page->baseSize().width()));

        page->renderIfNeeded(dpi, lowQualityPass ? PdfPageWidget::RenderQuality::Fast
                                                 : PdfPageWidget::RenderQuality::High);
        ++rendered;
    }

    bool needNextPass{};

    if (!m_warmupDone && rendered >= kMaxPagesPerPass) {
        m_warmupDone = true;
        needNextPass = true;
    } else if (rendered > 0 && m_warmupDone) {
        needNextPass = true;
    }

    m_rendering = false;

    if (needNextPass) {
        QTimer::singleShot(0, m_rootWidget, [this] { renderVisiblePages(); });
    }
}

void PdfViewer::updatePageScaleToFitWidth() {
    auto* vp = m_scrollArea->viewport();
    int const viewportW = vp->width();
    if (viewportW <= 0) { return; }

    constexpr int margin = 16;

    int const count = m_pagesLayout->count();
    for (int i = 0; i < count; ++i) {
        auto* page = qobject_cast<PdfPageWidget*>(m_pagesLayout->itemAt(i)->widget());
        if (page == nullptr) { continue; }

        double const scale =
            static_cast<double>(viewportW - margin) / static_cast<double>(page->baseSize().width());

        page->setScale(scale);
    }
}

void PdfViewer::createPageWidgetsStep() {
    if (!m_document) { return; }

    constexpr int kPagesPerBatch = 2;

    int const total = m_document->pages();
    int created{};

    while (m_pageCreateIndex < total && created < kPagesPerBatch) {
        auto page = std::unique_ptr<poppler::page>(m_document->create_page(m_pageCreateIndex));
        if (page) {
            auto* widget = new PdfPageWidget(std::move(page), m_pagesContainer);
            m_pagesLayout->addWidget(widget);

            auto* vp = m_scrollArea->viewport();
            constexpr int margin = 16;

            int const viewportW = vp->width();
            if (viewportW > 0) {
                double const scale = static_cast<double>(viewportW - margin) /
                                     static_cast<double>(widget->baseSize().width());

                widget->setScale(scale);
            }
        }
        ++m_pageCreateIndex;
        ++created;
    }

    if (m_pageCreateIndex < total) {
        QTimer::singleShot(0, m_rootWidget, [this] { createPageWidgetsStep(); });
    } else {
        m_pagesLayout->addStretch();
        updatePageScaleToFitWidth();
        renderVisiblePages();
    }
}

void PdfViewer::createAndRenderFirstPageImmediately() {
    if (!m_document) { return; }

    auto page = std::unique_ptr<poppler::page>(m_document->create_page(0));
    if (!page) { return; }

    auto* widget = new PdfPageWidget(std::move(page), m_pagesContainer);
    m_pagesLayout->addWidget(widget);

    updatePageScaleToFitWidth();

    auto* vp = m_scrollArea->viewport();
    double const dpi = std::min<double>(vp->logicalDpiX(), 144.0);

    widget->renderIfNeeded(dpi, PdfPageWidget::RenderQuality::Fast);
    QTimer::singleShot(0, m_rootWidget, [this] { renderVisiblePages(); });
}
