#pragma once

#include <wrl.h>
#include <wrl/client.h>
#include <WebView2.h>
#include <QWidget>
#include <QString>
#include <QUrl>

#include "ContentMode.hpp"

class WebView2Widget final : public QWidget {
        Q_OBJECT

    public:
        explicit WebView2Widget(QWidget* parent = nullptr);
        ~WebView2Widget() override;

        void setContentMode(ContentMode mode, const QUrl &baseUrl) noexcept {
            m_contentMode = mode;
            m_baseUrl = baseUrl;
        }

        void loadFile(const QString &path);
        void loadUrl(const QUrl &url);
        void find(const QString &text, bool backward = false);

    protected:
        void resizeEvent(QResizeEvent* e) override;
        void showEvent(QShowEvent* e) override;

    private:
        void initWebView();

        bool m_initialized{};
        QString m_pendingFile;

        QUrl m_pendingUrl;

        ContentMode m_contentMode{ContentMode::htmlFile};
        QUrl m_baseUrl;

        Microsoft::WRL::ComPtr<ICoreWebView2Environment> m_env;
        Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_controller;
        Microsoft::WRL::ComPtr<ICoreWebView2> m_webview;
};
