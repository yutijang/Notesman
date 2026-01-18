#pragma once

class QWidget;
class QToolBar;

class IResourceViewer {
    public:
        virtual ~IResourceViewer() = default;

        virtual QWidget* widget() = 0;
        [[nodiscard]] virtual bool isEditable() const = 0;
        [[nodiscard]] virtual bool hasUnsavedChanges() const = 0;
        virtual bool onClose(QWidget* parent) = 0;
        virtual void setupToolbar(QToolBar* toolbar) = 0;
};
