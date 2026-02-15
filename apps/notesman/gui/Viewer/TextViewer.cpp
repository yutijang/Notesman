#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QFont>
#include <QMessageBox>
#include <QToolBar>
#include <QColor>
#include <Qt>
#include <QSizePolicy>
#include <QObject>
#include <QLineEdit>
#include <QInputDialog>
#include <QTextCursor>
#include <sqlite3.h>

#include "TextViewer.hpp"
#include "CodeEditorLineHighlighter.hpp"
#include "PlainTextEdit.hpp"
#include "ResourceViewService.hpp"
#include "cpphighlighter.hpp"
#include "cpphighlightertheme.hpp"
#include "UiConstants.hpp"
#include "DialogUtils.hpp"
#include "helper.hpp"

TextViewer::TextViewer(sqlite3_int64 resourceId, bool editable, ResourceViewService& viewService,
                       UiConst::Theme theme, QWidget* parent)
    : m_resourceId(resourceId), m_editable(editable), m_viewService(viewService),
      m_currentTheme(theme) {
    m_rootWidget = new QWidget(parent);

    // ===== Editor =====
    setupEditor();
    // ===== Load content =====
    loadContent();
}

void TextViewer::setupEditor() {
    m_editor = new PlainTextEdit(m_rootWidget);

    m_editor->setReadOnly(!m_editable);

    if (m_editable) {
        m_editor->setTextInteractionFlags(Qt::TextEditorInteraction);
    } else {
        m_editor->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    }

    m_editor->setFont(QFont("JetBrains Mono", UiConst::FONT_SIZE));
    m_editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // ===== Layout =====
    auto* layout = new QVBoxLayout(m_rootWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_editor);
}

QWidget* TextViewer::widget() {
    return m_rootWidget;
}

bool TextViewer::isEditable() const {
    return m_editable;
}

bool TextViewer::hasUnsavedChanges() const {
    if (!isEditable() || m_editor == nullptr) { return false; }

    return m_editor->toPlainText() != m_originalContent;
}

bool TextViewer::onClose(QWidget* parent) {
    if (!hasUnsavedChanges()) { return true; }

    auto const reply = DialogUtils::showQuestion(parent, QObject::tr("Unsaved changes"),
                                                 QObject::tr("Save changes before closing?"));

    if (reply == QMessageBox::Cancel) { return false; }

    if (reply == QMessageBox::Yes) {
        auto const content = m_editor->toPlainText();
        m_viewService.saveTextResource(m_resourceId, content);
        m_originalContent = content;
    }

    return true;
}

void TextViewer::setupToolbar(QToolBar* toolbar) {
    if (toolbar == nullptr) { return; }

    // ===== Save =====
    auto* actionSave = toolbar->addAction(QObject::tr("Save"));
    actionSave->setIcon(QIcon(":/icons/save.ico"));
    actionSave->setEnabled(isEditable());

    QObject::connect(actionSave, &QAction::triggered, toolbar, [this]() {
        if (!isEditable()) { return; }

        m_originalContent = m_editor->toPlainText();
        m_viewService.saveTextResource(m_resourceId, m_originalContent);
    });

    // ===== Reload =====
    auto* actionReload = toolbar->addAction(QObject::tr("Reload"));
    actionReload->setIcon(QIcon(":/icons/load.ico"));

    QObject::connect(actionReload, &QAction::triggered, toolbar, [this]() { loadContent(); });

    // ===== Toggle Syntax Highlight =====
    auto* actionToggleSH = toolbar->addAction(QObject::tr("Toggle"));
    actionToggleSH->setCheckable(true);
    actionToggleSH->setChecked(m_isAppliedSH);

    QIcon toggleIcon;
    QPixmap const pixmap(":/icons/syntax.ico");
    toggleIcon.addPixmap(pixmap, QIcon::Normal, QIcon::On);
    toggleIcon.addPixmap(toggleIcon.pixmap(pixmap.size(), QIcon::Disabled), QIcon::Normal,
                         QIcon::Off);
    actionToggleSH->setIcon(toggleIcon);

    QObject::connect(
        actionToggleSH, &QAction::toggled, toolbar, [this, actionToggleSH](bool checked) {
            m_isAppliedSH = checked;
            actionToggleSH->setToolTip(checked ? QObject::tr("Disable syntax highlighting")
                                               : QObject::tr("Enable syntax highlighting"));

            if (checked) {
                applySyntaxHighlightingTheme();
            } else {
                disableSyntaxHighlightingTheme();
            }
        });

    actionToggleSH->toggled(m_isAppliedSH);

    // ===== Search =====
    auto* actionSearch = toolbar->addAction(QObject::tr("Search"));
    actionSearch->setIcon(QIcon(":/icons/search.ico"));
    actionSearch->setShortcut(QKeySequence::Find);
    actionSearch->setToolTip(QObject::tr("Search (Ctrl+F)"));

    QObject::connect(actionSearch, &QAction::triggered, toolbar, [this]() { startSearch(); });

    // Find next / previous
    auto* actFindNext = new QAction(m_rootWidget);
    actFindNext->setShortcut(Qt::Key_F3);
    actFindNext->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(actFindNext, &QAction::triggered, m_rootWidget, [this]() { findNext(); });

    auto* actFindPrev = new QAction(m_rootWidget);
    actFindPrev->setShortcut(Qt::SHIFT | Qt::Key_F3);
    actFindPrev->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    QObject::connect(actFindPrev, &QAction::triggered, m_rootWidget, [this]() { findPrevious(); });

    m_rootWidget->addAction(actFindNext);
    m_rootWidget->addAction(actFindPrev);
}

void TextViewer::startSearch() {
    bool ok{};
    QString const text =
        QInputDialog::getText(m_rootWidget, QObject::tr("Search"), QObject::tr("Find:"),
                              QLineEdit::Normal, m_lastSearchText, &ok);

    if (!ok || text.isEmpty()) { return; }

    m_lastSearchText = text;
    findNext();
}

void TextViewer::findNext() {
    if (m_lastSearchText.isEmpty()) { return; }

    if (!m_editor->find(m_lastSearchText)) {
        // wrap around
        QTextCursor c = m_editor->textCursor();
        c.movePosition(QTextCursor::Start);
        m_editor->setTextCursor(c);
        m_editor->find(m_lastSearchText);
    }
}

void TextViewer::findPrevious() {
    if (m_lastSearchText.isEmpty()) { return; }

    if (!m_editor->find(m_lastSearchText, QTextDocument::FindBackward)) {
        QTextCursor c = m_editor->textCursor();
        c.movePosition(QTextCursor::End);
        m_editor->setTextCursor(c);
        m_editor->find(m_lastSearchText, QTextDocument::FindBackward);
    }
}

void TextViewer::loadContent() {
    auto const textContent = m_viewService.loadTextResource(m_resourceId);
    if (!textContent) {
        m_editor->setPlainText(QObject::tr("No content available."));
        return;
    }

    m_originalContent = *textContent;

    m_editor->setUpdatesEnabled(false);

    if (Utils::looksLikeCppCode(m_originalContent.toStdString())) {
        setupHighlighter();
        m_isAppliedSH = true;
    }

    applyLineHighlighter();

    m_editor->setPlainText(m_originalContent);

    m_editor->setUpdatesEnabled(true);
    m_editor->update();
}

void TextViewer::applySyntaxHighlightingTheme() {
    if (m_editor == nullptr) { return; }

    // Chọn theme tô màu
    CppHighlighterTheme const hlTheme =
        (m_currentTheme == UiConst::Theme::Light) ? createLightTheme() : createDarkTheme();

    auto* doc = m_editor->document();

    // Nếu chưa có highlighter thì tạo mới
    if (m_cppHighlighter == nullptr) {
        m_cppHighlighter = new CppHighlighter(doc, hlTheme);
    } else {
        m_cppHighlighter->stopGradualRehighlight();
        m_cppHighlighter->setTheme(hlTheme);
    }

    // Áp dụng highlight dần (mượt, không đơ)
    m_cppHighlighter->rehighlightGradually(doc,
                                           20, // NOLINT(readability-magic-numbers)
                                           4);
}

void TextViewer::disableSyntaxHighlightingTheme() {
    if (m_editor == nullptr) { return; }

    if (m_cppHighlighter != nullptr) { m_cppHighlighter->stopGradualRehighlight(); }

    delete m_cppHighlighter;
    m_cppHighlighter = nullptr;
}

void TextViewer::applyLineHighlighter() {
    if (m_lineHighlighter != nullptr) { return; }

    m_lineHighlighter = new CodeEditorLineHighlighter(m_editor);
    if (m_currentTheme == UiConst::Theme::Light) {
        m_lineHighlighter->setColors(QColor("#dBdBdB"), QColor("#efefef"));
    } else {
        m_lineHighlighter->setColors(QColor("#2f2f2f"), QColor("#2a2a2a"));
    }
}

void TextViewer::setupHighlighter() {
    if (m_editor == nullptr) { return; }

    CppHighlighterTheme const hlTheme =
        (m_currentTheme == UiConst::Theme::Dark) ? createDarkTheme() : createLightTheme();

    auto* doc = m_editor->document();

    if (m_cppHighlighter == nullptr) {
        m_cppHighlighter = new CppHighlighter(doc, hlTheme);
    } else {
        m_cppHighlighter->stopGradualRehighlight();
        m_cppHighlighter->setTheme(hlTheme);
    }

    m_cppHighlighter->rehighlightGradually(doc,
                                           20, // NOLINT(readability-magic-numbers)
                                           4);
}
