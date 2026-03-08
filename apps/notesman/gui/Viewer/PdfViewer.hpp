#pragma once

#include "IResourceViewer.hpp"

#include <QFutureWatcher>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <memory>

namespace poppler {
    class document;
} // namespace poppler

class QScrollArea;
class QVBoxLayout;
class QToolBar;

class PdfViewer final : public IResourceViewer {
    public:
        explicit PdfViewer(QString pdfPath, QWidget* parent = nullptr);
        ~PdfViewer() override;

        void updatePageScaleToFitWidth();
        void onScroll();

    private:
        // IResourceViewer
        QWidget* widget() override;
        bool onClose(QWidget* parent) override;
        void setupToolbar(QToolBar* toolbar) override;

        void setupUi(QWidget* parent);
        void loadDocument();
        void clearPages();
        void createPageWidgets();
        void renderVisiblePages();
        void createPageWidgetsStep();
        void createAndRenderFirstPageImmediately();

        QTimer m_scrollDebounce;
        bool m_rendering{};
        int m_lastScrollY{};
        int m_pageCreateIndex{};
        bool m_warmupDone{};

        QString m_pdfPath;
        QWidget* m_rootWidget{};
        QScrollArea* m_scrollArea{};
        QWidget* m_pagesContainer{};
        QVBoxLayout* m_pagesLayout{};

        std::shared_ptr<poppler::document> m_document;
        QFutureWatcher<std::shared_ptr<poppler::document>> m_docWatcher;
};

class PdfViewerRootWidget final : public QWidget {
    public:
        explicit PdfViewerRootWidget(PdfViewer* viewer, QWidget* parent = nullptr)
            : QWidget(parent), m_viewer(viewer) {}

    protected:
        void resizeEvent(QResizeEvent* e) override {
            QWidget::resizeEvent(e);
            m_viewer->updatePageScaleToFitWidth();
            m_viewer->onScroll(); // debounce render
        }

    private:
        PdfViewer* m_viewer;
};
