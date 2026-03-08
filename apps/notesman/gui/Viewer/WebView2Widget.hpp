#pragma once

#include "ContentMode.hpp"

#include <QString>
#include <QUrl>
#include <QWidget>
#include <WebView2.h>
#include <wrl.h>
#include <wrl/client.h>

class WebView2Widget final : public QWidget {
        Q_OBJECT

    public:
        explicit WebView2Widget(QWidget* parent = nullptr);
        ~WebView2Widget() override;

        void setContentMode(ContentMode mode, QUrl const& baseUrl) noexcept {
            m_contentMode = mode;
            m_baseUrl = baseUrl;
        }

        void loadFile(QString const& path);
        void loadUrl(QUrl const& url);
        void find(QString const& text, bool backward = false);

    protected:
        void resizeEvent(QResizeEvent* e) override;
        void showEvent(QShowEvent* e) override;

    private:
        void initWebView();

        bool m_initialized{};
        QString m_pendingFile;

        QUrl m_pendingUrl;

        ContentMode m_contentMode{ContentMode::HtmlFile};
        QUrl m_baseUrl;

        Microsoft::WRL::ComPtr<ICoreWebView2Environment> m_env;
        Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_controller;
        Microsoft::WRL::ComPtr<ICoreWebView2> m_webview;
};
