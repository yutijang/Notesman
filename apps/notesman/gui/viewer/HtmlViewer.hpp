#pragma once

#include <QString>

#include "IResourceViewer.hpp"
#include "ISearchable.hpp"

class QWidget;
class QTextBrowser;

class HtmlViewer final : public IResourceViewer,
                         public ISearchable {
    public:
        explicit HtmlViewer(QString path, QWidget* parent = nullptr);

        ~HtmlViewer() override = default;

        // ===== IResourceViewer =====
        QWidget* widget() override;
        [[nodiscard]] bool isEditable() const override;
        [[nodiscard]] bool hasUnsavedChanges() const override;
        bool onClose(QWidget* parent) override;
        void setupToolbar(QToolBar* toolbar) override;

    private:
        // ISearchable
        void startSearch() override;
        void findNext() override;
        void findPrevious() override;

        void setupView();
        void loadFile();

        QString m_htmlPath;

        QWidget* m_rootWidget{};
        QTextBrowser* m_browser{};

        QString m_lastSearchText;
};
