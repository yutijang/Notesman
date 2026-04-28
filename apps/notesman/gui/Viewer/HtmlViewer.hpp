#pragma once

#include "ContentMode.hpp"
#include "IResourceViewer.hpp"

#include <QString>
#include <QUrl>
#include <memory>

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
        static std::unique_ptr<HtmlViewer> createFromFile(QString title, QString path,
                                                          ContentMode mode, QWidget* parent);

        // url
        static std::unique_ptr<HtmlViewer> createFromUrl(QString title, QUrl url, ContentMode mode,
                                                         QWidget* parent);

        ~HtmlViewer() override = default;

#ifdef Q_OS_LINUX
        [[nodiscard]] bool usesExternalWindow() const override { return true; }

        [[nodiscard]] QProcess* externalProcess() const override { return m_process; }
#endif

    private:
        HtmlViewer(QString title, QWidget* parent);
        bool initFromFile(QString path, ContentMode mode);
        bool initFromUrl(QUrl url, ContentMode mode);

        // ===== IResourceViewer =====
        QWidget* widget() override;
        bool onClose(QWidget* parent) override;
        void setupToolbar(QToolBar* toolbar) override;

        // ISearchable
        void startSearch() override;
        void findNext() override;
        void findPrevious() override;

        void setupView();

        [[nodiscard]] bool supportsSearch() const;

        QString m_htmlPath;
        QString m_title;
        QUrl m_url;

        QWidget* m_rootWidget{};
        QString m_lastSearchText;

#if defined(Q_OS_WIN)
        WebView2Widget* m_view{};
#elif defined(Q_OS_LINUX)
        QProcess* m_process{};
#endif
};
