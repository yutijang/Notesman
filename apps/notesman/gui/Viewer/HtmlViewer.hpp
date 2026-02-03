#pragma once

#include <QString>
#include <QUrl>

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
        // load từ file
        explicit HtmlViewer(QString title, QString path, QWidget* parent = nullptr);

        // load từ memory
        explicit HtmlViewer(QString title, QString htmlContent, bool fromMemory,
                            QWidget* parent = nullptr);

        // url
        explicit HtmlViewer(QString title, QUrl url, QWidget* parent = nullptr);

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
        void loadFromMemory();
        void loadUrl();

        [[nodiscard]] bool supportsSearch() const;

        QString m_htmlPath;
        QString m_htmlContent;
        QString m_title;
        QUrl m_url;
        bool m_fromMemory{};

        QWidget* m_rootWidget{};

#if defined(Q_OS_WIN)
        WebView2Widget* m_view{};
#elif defined(Q_OS_LINUX)
        QProcess* m_process{};
#endif

        QString m_lastSearchText;
};
