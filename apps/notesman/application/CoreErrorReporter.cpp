#include "CoreErrorReporter.hpp"

#include "Logger.hpp"

void CoreErrorReporter::showError(QString const& title, QString const& message) {
    Log::err("[{}] {}", title.toStdString(), message.toStdString());
}

void CoreErrorReporter::showInfo(QString const& title, QString const& message) {
    Log::info("[{}] {}", title.toStdString(), message.toStdString());
}

bool CoreErrorReporter::askQuestion(QString const& title, QString const& question) {
    Log::err("[{}] {} — no user input available in headless mode, declining automatically.",
             title.toStdString(), question.toStdString());
    return false;
}
