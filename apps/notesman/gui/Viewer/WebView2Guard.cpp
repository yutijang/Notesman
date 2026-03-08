#include "WebView2Guard.hpp"

#include <string>

WebView2Guard& WebView2Guard::instance() {
    static WebView2Guard inst;
    return inst;
}

WebView2Guard::WebView2Guard() {
    loadLoader();
    if (m_loader == nullptr) {
        m_status = Status::LoaderMissing;
        return;
    }

    resolveFunctions();
    if ((m_getVersion == nullptr) || (m_createEnv == nullptr)) {
        m_status = Status::ApiUnavailable;
        return;
    }

    checkRuntime();
}

WebView2Guard::~WebView2Guard() {
    if (m_loader != nullptr) { FreeLibrary(m_loader); }
}

void WebView2Guard::loadLoader() {
    m_loader = LoadLibraryW(L"WebView2Loader.dll");
    if (m_loader == nullptr) { m_status = Status::LoaderMissing; }
}

void WebView2Guard::resolveFunctions() {
    if (m_loader == nullptr) { return; }

    m_getVersion = reinterpret_cast<FnGetVersion>(
        GetProcAddress(m_loader, "GetAvailableCoreWebView2BrowserVersionString"));

    m_createEnv = reinterpret_cast<FnCreateEnv>(
        GetProcAddress(m_loader, "CreateCoreWebView2EnvironmentWithOptions"));
}

void WebView2Guard::checkRuntime() {
    LPWSTR version = nullptr;
    const HRESULT hr = m_getVersion(nullptr, &version);

    if (FAILED(hr) || (version == nullptr)) {
        m_status = Status::RuntimeMissing;
        return;
    }

    m_version = version;
    CoTaskMemFree(version);

    m_status = Status::Ok;
}

WebView2Guard::Status WebView2Guard::status() const noexcept {
    return m_status;
}

bool WebView2Guard::available() const noexcept {
    return m_status == Status::Ok;
}

std::wstring WebView2Guard::runtimeVersion() const {
    return m_version;
}

HRESULT WebView2Guard::createEnvironment(
    wchar_t const* userDataDir,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* handler) const {
    if (!available()) { return E_FAIL; }

    return m_createEnv(nullptr,     // browserExecutableFolder
                       userDataDir, // userDataFolder
                       nullptr,     // options
                       handler);
}
