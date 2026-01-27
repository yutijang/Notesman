#pragma once

#include <QString>

#include "IResourceViewer.hpp"

class QWidget;
class QTextBrowser;
class WebView2Widget;

#ifdef Q_OS_LINUX
class QProcess;
#endif

class HtmlViewer final : public IResourceViewer,
                         public ISearchable {
    public:
        explicit HtmlViewer(QString title, QString path, QWidget* parent = nullptr);

        ~HtmlViewer() override = default;

#ifdef Q_OS_LINUX
        [[nodiscard]] bool usesExternalWindow() const override { return true; }
#endif

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
        QString m_title;

        QWidget* m_rootWidget{};
        // QTextBrowser* m_browser{};

#if defined(Q_OS_WIN)
        WebView2Widget* m_view{};
#elif defined(Q_OS_LINUX)
        QProcess* m_process{};
#endif

        QString m_lastSearchText;
};
