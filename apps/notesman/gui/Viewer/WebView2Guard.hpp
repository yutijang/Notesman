#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <WebView2.h>

class WebView2Guard final {
    public:
        enum class Status : std::uint8_t { ok, loaderMissing, runtimeMissing, apiUnavailable };

        static WebView2Guard &instance();

        [[nodiscard]] Status status() const noexcept;
        [[nodiscard]] bool available() const noexcept;

        HRESULT createEnvironment(
            const wchar_t* userDataDir,
            ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* handler) const;

        [[nodiscard]] std::wstring runtimeVersion() const;

    private:
        WebView2Guard();
        ~WebView2Guard();

        void loadLoader();
        void resolveFunctions();
        void checkRuntime();

        HMODULE m_loader{};
        Status m_status{Status::loaderMissing};

        using FnGetVersion = HRESULT(STDMETHODCALLTYPE*)(PCWSTR, LPWSTR*);
        using FnCreateEnv =
            HRESULT(WINAPI*)(LPCWSTR, LPCWSTR, ICoreWebView2EnvironmentOptions*,
                             ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

        FnGetVersion m_getVersion{};
        FnCreateEnv m_createEnv{};

        std::wstring m_version;
};
