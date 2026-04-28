#pragma once

class QWidget;
class QToolBar;
class QProcess;

// Giao diện cơ bản nhất - Mọi Viewer đều phải có
class IResourceViewer {
    public:
        virtual ~IResourceViewer() = default;
        virtual QWidget* widget() = 0;
        virtual bool onClose(QWidget* parent) = 0;
        virtual void setupToolbar(QToolBar* toolbar) = 0;

        [[nodiscard]] virtual bool usesExternalWindow() const { return false; }

#ifdef Q_OS_LINUX
        [[nodiscard]] virtual QProcess* externalProcess() const { return nullptr; }
#endif
};

// Giao diện dành cho các Viewer có thể chỉnh sửa
class IEditable {
    public:
        virtual ~IEditable() = default;
        [[nodiscard]] virtual bool isEditable() const = 0;
        [[nodiscard]] virtual bool hasUnsavedChanges() const = 0;
};

class ISearchable {
    public:
        virtual ~ISearchable() = default;

        virtual void startSearch() = 0;  // Ctrl + F
        virtual void findNext() = 0;     // F3
        virtual void findPrevious() = 0; // Shift + F3
};
