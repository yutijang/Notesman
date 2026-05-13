#pragma once

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)

namespace FileAssociation {

// Windows comment
// Quản lý Windows file association cho .rvpk
// Dùng HKCU — không cần admin privilege
// Kiểm tra association hiện tại có trỏ đúng vào exe đang chạy không
// false = chưa register hoặc path bị stale (app bị di chuyển)
[[nodiscard]] bool isUpToDate();

// Windows comment
// Ghi/cập nhật association vào registry
// Trả về true nếu thành công
[[nodiscard]] bool registerAssociation();

// Windows comment
// Xoá association khỏi registry
void unregisterAssociation();

} // namespace FileAssociation

#endif
