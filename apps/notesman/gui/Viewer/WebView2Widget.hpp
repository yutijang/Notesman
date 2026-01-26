#pragma once

#include <wrl.h>
#include <wrl/client.h>
#include <WebView2.h>
#include <QWidget>
#include <QString>

class WebView2Widget final : public QWidget {
        Q_OBJECT

    public:
        explicit WebView2Widget(QWidget* parent = nullptr);
        ~WebView2Widget() override;

        void loadFile(const QString &path);
        void find(const QString &text, bool backward = false);

    protected:
        void resizeEvent(QResizeEvent* e) override;
        void showEvent(QShowEvent* e) override;

    private:
        void initWebView();

        bool m_initialized{};
        QString m_pendingFile;

        Microsoft::WRL::ComPtr<ICoreWebView2Environment> m_env;
        Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_controller;
        Microsoft::WRL::ComPtr<ICoreWebView2> m_webview;
};
