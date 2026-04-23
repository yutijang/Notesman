#pragma once

#include <QString>

class ICoreErrorHandler {
    public:
        virtual ~ICoreErrorHandler() = default;
        virtual void showError(QString const& title, QString const& message) = 0;
        virtual bool askQuestion(QString const& title, QString const& question) = 0;
        virtual void showInfo(QString const& title, QString const& message) = 0;
};
