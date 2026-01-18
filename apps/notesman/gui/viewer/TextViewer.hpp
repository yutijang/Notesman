#pragma once

#include <QWidget>
#include <QString>
#include <sqlite3.h>

#include "IResourceViewer.hpp"
#include "ISearchable.hpp"
#include "ResourceViewService.hpp"
#include "Theme.hpp"

class QWidget;
class PlainTextEdit;
class CppHighlighter;
class CodeEditorLineHighlighter;

class TextViewer final : public IResourceViewer,
                         public ISearchable {
    public:
        TextViewer(sqlite3_int64 resourceId, bool editable, ResourceViewService &viewService,
                   Theme theme, QWidget* parent = nullptr);

        ~TextViewer() override = default;

        // ===== IResourceViewer =====
        QWidget* widget() override;
        [[nodiscard]] bool isEditable() const override;
        [[nodiscard]] bool hasUnsavedChanges() const override;
        bool onClose(QWidget* parent) override;
        void setupToolbar(QToolBar* toolbar) override;

    private:
        void createUi(QWidget* parent);
        void setupEditor();
        void loadContent();

        void setupHighlighter();
        void applyLineHighlighter();
        void applySyntaxHighlightingTheme();
        void disableSyntaxHighlightingTheme();

        // ISearchable
        void startSearch() override;
        void findNext() override;
        void findPrevious() override;

        // void findNext(const QString &text);

        sqlite3_int64 m_resourceId;
        bool m_editable{};
        ResourceViewService &m_viewService;
        QString m_originalContent;
        Theme m_currentTheme{};
        bool m_isAppliedSH{};

        QWidget* m_rootWidget{};
        PlainTextEdit* m_editor{};
        CppHighlighter* m_cppHighlighter{};
        CodeEditorLineHighlighter* m_lineHighlighter{};

        QString m_lastSearchText;
};
