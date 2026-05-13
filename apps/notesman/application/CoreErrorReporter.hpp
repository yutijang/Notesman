#pragma once

#include "ICoreErrorHandler.hpp"

#include <QString>

// Console/headless implementation của ICoreErrorHandler.
// Dùng cho PackerLauncher và các context không có GUI.
// askQuestion luôn trả về false — câu hỏi Yes/No không có ý nghĩa khi không có user.
class CoreErrorReporter final : public ICoreErrorHandler {
  public:
    CoreErrorReporter() = default;

    void showError(QString const& title, QString const& message) override;
    void showInfo(QString const& title, QString const& message) override;

    // Luôn trả về false. Caller nhận InitFailureReason tương ứng và tự quyết định exit.
    bool askQuestion(QString const& title, QString const& question) override;
};
