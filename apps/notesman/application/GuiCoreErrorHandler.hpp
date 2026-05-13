#pragma once

#include "ICoreErrorHandler.hpp"

#include <QString>

class QWidget;

class GuiCoreErrorHandler final : public ICoreErrorHandler {
  public:
    explicit GuiCoreErrorHandler(QWidget* parent) : m_parent(parent) {}

    void showError(QString const& title, QString const& message) override;
    void showInfo(QString const& title, QString const& message) override;
    bool askQuestion(QString const& title, QString const& question) override;

  private:
    QWidget* m_parent;
};
