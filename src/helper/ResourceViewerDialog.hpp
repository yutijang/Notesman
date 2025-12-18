#pragma once

#include <sqlite3.h>
#include <QDialog>
#include <QString>

#include "Theme.hpp"
#include "model.hpp"

class QWidget;
class PlainTextEdit;
class ResourceViewService;
class QCloseEvent;
class CppHighlighter;
class CodeEditorLineHighlighter;

class ResourceViewerDialog final : public QDialog {
        Q_OBJECT

    public:
        explicit ResourceViewerDialog(sqlite3_int64 id, const QString &title, ResourceType type,
                                      Theme theme, ResourceViewService &viewService,
                                      QWidget* parent = nullptr);
        ~ResourceViewerDialog() override = default;

    protected:
        void closeEvent(QCloseEvent* event) override;

    private:
        void setupUi(const QString &title);
        void loadContent();
        void setupActions();
        void applyLineHighlighter();
        void setupHighlighter();
        void applySyntaxHighlightingTheme();
        void disableSyntaxHighlightingTheme();

        sqlite3_int64 m_resourceId;
        ResourceType m_type{};
        ResourceViewService &m_viewService;
        QString m_originalContent;
        PlainTextEdit* m_editor{};
        CppHighlighter* m_cppHighlighter{};
        CodeEditorLineHighlighter* m_lineHighlighter{};
        Theme m_currentTheme{};
        bool m_isAppliedSH{};
};
