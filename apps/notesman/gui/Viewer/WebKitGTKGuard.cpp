#include "WebKitGTKGuard.hpp"

#include <dlfcn.h>

namespace {
    constexpr char const* K_WEB_KIT_GTK_SONAME = "libwebkit2gtk-4.1.so.0";
} // namespace

WebKitGTKGuard& WebKitGTKGuard::instance() {
    static WebKitGTKGuard g;
    return g;
}

WebKitGTKGuard::WebKitGTKGuard() {
    // RTLD_LAZY: đủ để kiểm tra loader resolve
    // RTLD_LOCAL: không pollute symbol table
    m_handle = dlopen(K_WEB_KIT_GTK_SONAME, RTLD_LAZY | RTLD_LOCAL);
}

WebKitGTKGuard::~WebKitGTKGuard() {
    if (m_handle != nullptr) { dlclose(m_handle); }
}

bool WebKitGTKGuard::available() const noexcept {
    return m_handle != nullptr;
}
