#pragma once

class WebKitGTKGuard {
    public:
        static WebKitGTKGuard& instance();

        [[nodiscard]] bool available() const noexcept;

    private:
        WebKitGTKGuard();
        ~WebKitGTKGuard();

        void* m_handle = nullptr;
};
