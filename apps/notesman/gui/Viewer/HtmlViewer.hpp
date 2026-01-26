#pragma once

#include <QString>

#include "IResourceViewer.hpp"

class QWidget;
class QTextBrowser;
class WebView2Widget;

class HtmlViewer final : public IResourceViewer,
                         public ISearchable {
    public:
        explicit HtmlViewer(QString path, QWidget* parent = nullptr);

        ~HtmlViewer() override = default;

    private:
        // ===== IResourceViewer =====
        QWidget* widget() override;
        bool onClose(QWidget* parent) override;
        void setupToolbar(QToolBar* toolbar) override;

        // ISearchable
        void startSearch() override;
        void findNext() override;
        void findPrevious() override;

        void setupView();
        void loadFile();

        QString m_htmlPath;

        QWidget* m_rootWidget{};
        // QTextBrowser* m_browser{};

#ifdef Q_OS_WIN
        WebView2Widget* m_view{};
#endif

        QString m_lastSearchText;
};
