#include "application/GuiCoreErrorHandler.hpp"

#include "helper/DialogUtils.hpp"

#include <QMessageBox>
#include <QString>

void GuiCoreErrorHandler::showError(QString const& title, QString const& message) {
    DialogUtils::showError(m_parent, title, message);
}

void GuiCoreErrorHandler::showInfo(QString const& title, QString const& message) {
    DialogUtils::showInfo(m_parent, title, message);
}

bool GuiCoreErrorHandler::askQuestion(QString const& title, QString const& question) {
    return DialogUtils::showQuestion(m_parent, title, question) == QMessageBox::Yes;
}
