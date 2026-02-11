#include <windows.h>
#include <WebView2.h>
#include <wrl/client.h>
#include <wrl/event.h>

#include <QDebug>
#include <QWindow>
#include <QResizeEvent>
#include <QUrl>
#include <Qt>
#include <QWidget>
#include <QRect>
#include <QtAssert>
#include <QtLogging>
#include <QFileInfo>
#include <QStandardPaths>

#include "WebView2Widget.hpp"
#include "ContentMode.hpp"
#include "WebView2Guard.hpp"
#include "Logger.hpp"

using Microsoft::WRL::ComPtr;

static QString escapeJsString(QString s) {
    s.replace("\\", "\\\\");
    s.replace("\"", "\\\"");
    s.replace("\n", "\\n");
    s.replace("\r", "");
    return s;
}

WebView2Widget::WebView2Widget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
}

WebView2Widget::~WebView2Widget() {
    if (m_controller != nullptr) { m_controller->Close(); }
}

void WebView2Widget::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);

    if (!m_initialized) {
        m_initialized = true;
        initWebView();
    }
}

void WebView2Widget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);

    if (m_controller != nullptr) {
        const QRect r = rect();
        RECT rc{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
        m_controller->put_Bounds(rc);
    }
}

void WebView2Widget::initWebView() {
    HWND hwnd = reinterpret_cast<HWND>(winId());
    Q_ASSERT(hwnd);

    auto &guard = WebView2Guard::instance();
    if (!guard.available()) {
        Log::warn("WebView2 runtime not available");
        return;
    }

    QString userDataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/WebView2";

    guard.createEnvironment(
        userDataDir.toStdWString().c_str(),
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, hwnd](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(hr) || !env) {
                    Log::warn("WebView2 env create failed");
                    return hr;
                }

                m_env = env;

                env->CreateCoreWebView2Controller(
                    hwnd,
                    Microsoft::WRL::Callback<
                        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT chr, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(chr)) {
                                Log::warn("WebView2 controller create failed");
                                return chr;
                            }

                            m_controller = controller;
                            controller->get_CoreWebView2(&m_webview);

                            RECT bounds;
                            GetClientRect(reinterpret_cast<HWND>(winId()), &bounds);
                            m_controller->put_Bounds(bounds);

                            if (!m_pendingFile.isEmpty()) {
                                m_webview->Navigate(m_pendingFile.toStdWString().c_str());
                                m_pendingFile.clear();
                            } else if (m_pendingUrl.isValid()) {
                                const QString urlStr = m_pendingUrl.toString(QUrl::FullyEncoded);
                                m_webview->Navigate(urlStr.toStdWString().c_str());
                                m_pendingUrl = QUrl{};
                            }

                            // Settings
                            ComPtr<ICoreWebView2Settings> settings;
                            m_webview->get_Settings(&settings);
                            settings->put_IsScriptEnabled(TRUE);
                            settings->put_AreDefaultContextMenusEnabled(FALSE);
                            settings->put_IsZoomControlEnabled(FALSE);
                            settings->put_AreDevToolsEnabled(FALSE);
                            settings->put_IsStatusBarEnabled(FALSE);
                            settings->put_IsWebMessageEnabled(FALSE);

                            ComPtr<ICoreWebView2Settings3> settings3;
                            if (SUCCEEDED(settings.As(&settings3)) && settings3) {
                                settings3->put_AreBrowserAcceleratorKeysEnabled(FALSE);
                            }

                            m_webview->add_NewWindowRequested(
                                Microsoft::WRL::Callback<
                                    ICoreWebView2NewWindowRequestedEventHandler>(
                                    [](ICoreWebView2*,
                                       ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                                        args->put_Handled(TRUE);
                                        return S_OK;
                                    })
                                    .Get(),
                                nullptr);

                            m_webview->add_NavigationStarting(
                                Microsoft::WRL::Callback<
                                    ICoreWebView2NavigationStartingEventHandler>(
                                    [this](
                                        ICoreWebView2*,
                                        ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                        LPWSTR uri{};
                                        args->get_Uri(&uri);

                                        if (!uri) { return S_OK; }

                                        QUrl target = QUrl(QString::fromWCharArray(uri));
                                        CoTaskMemFree(uri);

                                        const QString scheme = target.scheme();

                                        switch (m_contentMode) {
                                            case ContentMode::HtmlFile:
                                            case ContentMode::Epub    : {
                                                // chỉ cho file://, data:, about:
                                                if (scheme == "file" || scheme == "data" ||
                                                    scheme == "about") {
                                                    return S_OK;
                                                }

                                                // CHẶN mọi link ngoài
                                                args->put_Cancel(TRUE);
                                                return S_OK;
                                            }

                                            case ContentMode::Url: {
                                                // chỉ cho http/https
                                                if (scheme != "http" && scheme != "https") {
                                                    args->put_Cancel(TRUE);
                                                    return S_OK;
                                                }

                                                // chỉ cho cùng host
                                                if (target.host() != m_baseUrl.host()) {
                                                    args->put_Cancel(TRUE);
                                                    return S_OK;
                                                }

                                                return S_OK;
                                            }
                                        }

                                        args->put_Cancel(TRUE);
                                        return S_OK;
                                    })
                                    .Get(),
                                nullptr);

                            ComPtr<ICoreWebView2_4> webview4;
                            if (FAILED(m_webview.As(&webview4)) || !webview4) {
                                // Runtime WebView2 quá cũ → không chặn download được
                                Log::warn("Runtime WebView2 quá cũ → không chặn download được");
                                return S_OK;
                            }
                            webview4->add_DownloadStarting(
                                Microsoft::WRL::Callback<ICoreWebView2DownloadStartingEventHandler>(
                                    [](ICoreWebView2*,
                                       ICoreWebView2DownloadStartingEventArgs* args) -> HRESULT {
                                        // Hủy download
                                        args->put_Cancel(TRUE);
                                        return S_OK;
                                    })
                                    .Get(),
                                nullptr);

                            return S_OK;
                        })
                        .Get());

                return S_OK;
            })
            .Get());
}

void WebView2Widget::loadFile(const QString &path) {
    QFileInfo fi(path);
    const QString absolutePath = fi.absoluteFilePath();

    if (!QFile::exists(absolutePath)) {
        Log::warn("HTML file not found: {}", absolutePath.toStdString());
        return;
    }

    const QUrl url = QUrl::fromLocalFile(absolutePath);

    if (m_webview == nullptr) {
        // WebView2 CHƯA SẴN SÀNG → ghi nhớ lại
        m_pendingFile = url.toString();
        return;
    }

    m_webview->Navigate(url.toString().toStdWString().c_str());
}

void WebView2Widget::find(const QString &text, bool backward) {
    if ((m_webview == nullptr) || text.isEmpty()) { return; }

    const QString js = QString("window.find(\"%1\", false, %2, true, false, true, false);")
                           .arg(escapeJsString(text), backward ? "true" : "false");

    m_webview->ExecuteScript(js.toStdWString().c_str(), nullptr);
}

void WebView2Widget::loadUrl(const QUrl &url) {
    if (!url.isValid()) {
        Log::warn("Invalid URL: {}", url.toString().toStdString());
        return;
    }

    if (url.scheme() != "http" && url.scheme() != "https") {
        Log::warn("Unsupported URL scheme: {}", url.toString().toStdString());
        return;
    }

    const QString urlStr = url.toString(QUrl::FullyEncoded);

    if (m_webview == nullptr) {
        // WebView2 chưa sẵn sàng
        m_pendingUrl = url;
        return;
    }

    m_webview->Navigate(urlStr.toStdWString().c_str());
}
