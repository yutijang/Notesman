#pragma once

#ifdef Q_OS_WIN
#include <QString>

// Quản lý Windows file association cho .rvpk
// Dùng HKCU — không cần admin privilege
namespace FileAssociation {
    // Kiểm tra association hiện tại có trỏ đúng vào exe đang chạy không
    // false = chưa register hoặc path bị stale (app bị di chuyển)
    [[nodiscard]] bool isUpToDate();

    // Ghi/cập nhật association vào registry
    // Trả về true nếu thành công
    [[nodiscard]] bool registerAssociation();

    // Xoá association khỏi registry
    void unregisterAssociation();
} // namespace FileAssociation
#endif
