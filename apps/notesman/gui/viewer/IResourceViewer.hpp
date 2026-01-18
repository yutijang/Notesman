#pragma once

class QWidget;
class QToolBar;

/**
 * @brief UI-level contract for all resource viewers (text, html, pdf, epub, ...)
 *
 * Responsibilities:
 *  - Owns and manages its content widget
 *  - Knows whether content is editable / modified
 *  - Participates in dialog lifecycle (close handling)
 *  - Optionally contributes actions to dialog toolbar
 *
 * Does NOT:
 *  - Know about dialogs
 *  - Know about sqlite / filesystem
 *  - Create or own windows
 */
class IResourceViewer {
    public:
        virtual ~IResourceViewer() = default;

        /**
         * @brief Main widget used to display the resource
         * Ownership stays with the viewer.
         */
        virtual QWidget* widget() = 0;

        /**
         * @brief Whether the content can be edited by user
         */
        [[nodiscard]] virtual bool isEditable() const = 0;

        /**
         * @brief Whether content has unsaved changes
         */
        [[nodiscard]] virtual bool hasUnsavedChanges() const = 0;

        /**
         * @brief Called when dialog is about to close
         *
         * Viewer should:
         *  - save data if needed
         *  - release resources
         */
        virtual bool onClose(QWidget* parent) = 0;

        /**
         * @brief Allow viewer to populate dialog toolbar
         *
         * Viewer may:
         *  - add save / reload / toggle actions
         *  - do nothing if no toolbar is needed
         */
        virtual void setupToolbar(QToolBar* toolbar) = 0;
};
