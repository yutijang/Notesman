#pragma once

#include <memory>
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
        static std::unique_ptr<HtmlViewer> createFromFile(QString title, QString path,
                                                          QWidget* parent);

        // load từ memory
        static std::unique_ptr<HtmlViewer> createFromMemory(QString title, QString html,
                                                            QWidget* parent);

        // url
        static std::unique_ptr<HtmlViewer> createFromUrl(QString title, QUrl url, QWidget* parent);

        ~HtmlViewer() override = default;

#ifdef Q_OS_LINUX
        [[nodiscard]] bool usesExternalWindow() const override { return true; }

        [[nodiscard]] QProcess* process() const { return m_process; }
#endif

    private:
        HtmlViewer(QString title, QWidget* parent);
        void initFromFile(QString path);
        void initFromMemory(QString html);
        void initFromUrl(QUrl url);

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
        QString m_htmlContent;
        QString m_title;
        QUrl m_url;
        bool m_fromMemory{};

        QWidget* m_rootWidget{};
        QString m_lastSearchText;

#if defined(Q_OS_WIN)
        WebView2Widget* m_view{};
#elif defined(Q_OS_LINUX)
        QProcess* m_process{};
#endif
};
