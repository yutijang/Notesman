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

    initWebView();
}

WebView2Widget::~WebView2Widget() {
    if (m_controller != nullptr) { m_controller->Close(); }
}

void WebView2Widget::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);

    if (m_pendingUrl.isValid() && (m_webview != nullptr)) {
        const QString urlStr = m_pendingUrl.toString(QUrl::FullyEncoded);
        m_webview->Navigate(urlStr.toStdWString().c_str());
        m_pendingUrl = QUrl{};
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

    QString userDataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/WebView2";

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataDir.toStdWString().c_str(), nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, hwnd](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(hr)) {
                    qWarning() << "WebView2 env create failed";
                    return hr;
                }

                m_env = env;

                env->CreateCoreWebView2Controller(
                    hwnd,
                    Microsoft::WRL::Callback<
                        ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT chr, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(chr)) {
                                qWarning() << "WebView2 controller create failed";
                                return chr;
                            }

                            m_controller = controller;
                            controller->get_CoreWebView2(&m_webview);

                            RECT bounds;
                            GetClientRect(reinterpret_cast<HWND>(winId()), &bounds);
                            m_controller->put_Bounds(bounds);

                            if (m_hasPendingHtml) {
                                m_webview->NavigateToString(m_pendingHtml.toStdWString().c_str());
                                m_pendingHtml.clear();
                                m_hasPendingHtml = false;
                            } else if (!m_pendingFile.isEmpty()) {
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
                                    [](ICoreWebView2*,
                                       ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                        LPWSTR uri{};
                                        args->get_Uri(&uri);

                                        if (!uri) { return S_OK; }

                                        QString url = QString::fromWCharArray(uri);
                                        CoTaskMemFree(uri);

                                        QUrl qurl(url);
                                        const QString scheme = qurl.scheme();

                                        if (scheme != "file" && scheme != "http" &&
                                            scheme != "https" && scheme != "about") {
                                            args->put_Cancel(TRUE);
                                        }

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
        qWarning() << "HTML file not found:" << absolutePath;
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

void WebView2Widget::loadHtml(const QString &html) {
    if (html.isEmpty()) { return; }

    if (m_webview == nullptr) {
        // WebView2 CHƯA SẴN SÀNG
        m_pendingHtml = html;
        m_hasPendingHtml = true;
        return;
    }

    m_webview->NavigateToString(html.toStdWString().c_str());
}

void WebView2Widget::loadUrl(const QUrl &url) {
    qDebug() << "WebView2Widget visible:" << isVisible() << "size:" << size();

    if (!url.isValid()) {
        qWarning() << "Invalid URL:" << url;
        return;
    }

    if (url.scheme() != "http" && url.scheme() != "https") {
        qWarning() << "Unsupported URL scheme:" << url;
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
