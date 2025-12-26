#include <QFont>
#include <qnamespace.h>
#include <qobject.h>
#include <qsizepolicy.h>
#include <qtmetamacros.h>
#include <sqlite3.h>
#include <QCloseEvent>
#include <QMessageBox>
#include <QString>
#include <QWidget>
#include <QDialog>
#include <QToolBar>
#include <QTimer>
#include <QStyle>
#include <QVBoxLayout>
#include <QIcon>
#include <QPixmap>

#include "ResourceViewerDialog.hpp"
#include "PlainTextEdit.hpp"
#include "helper.hpp"
#include "UiConstants.hpp"
#include "cpphighlightertheme.hpp"
#include "ResourceViewService.hpp"
#include "Theme.hpp"
#include "model.hpp"
#include "cpphighlighter.hpp"
#include "CodeEditorLineHighlighter.hpp"
#include "DialogUtils.hpp"

ResourceViewerDialog::ResourceViewerDialog(sqlite3_int64 id, const QString &title,
                                           ResourceType type, Theme theme,
                                           ResourceViewService &viewService, QWidget* parent)
    : QDialog(parent), m_resourceId(id), m_type(type), m_viewService(viewService),
      m_currentTheme(theme) {
    setAttribute(Qt::WA_DeleteOnClose);
    setupUi(title);
    loadContent();
    setupActions();
}

void ResourceViewerDialog::closeEvent(QCloseEvent* event) {
    const QString currentContent = m_editor->toPlainText();
    if (currentContent != m_originalContent) {
        const auto ret = DialogUtils::showQuestion(this, tr("Unsaved changes"),
                                                   tr("Save changes before closing?"));
        if (ret == QMessageBox::Yes) {
            m_viewService.saveTextResource(m_resourceId, currentContent);
        } else if (ret == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
    }

    QDialog::closeEvent(event);
}

void ResourceViewerDialog::applyLineHighlighter() {
    if (m_lineHighlighter != nullptr) { return; }

    m_lineHighlighter = new CodeEditorLineHighlighter(m_editor);
    if (m_currentTheme == Theme::light) {
        m_lineHighlighter->setColors(QColor("#dBdBdB"), QColor("#efefef"));
    } else {
        m_lineHighlighter->setColors(QColor("#2f2f2f"), QColor("#2a2a2a"));
    }
}

void ResourceViewerDialog::setupHighlighter() {
    if (m_editor == nullptr) { return; }

    const CppHighlighterTheme hlTheme =
        (m_currentTheme == Theme::dark) ? createDarkTheme() : createLightTheme();

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

void ResourceViewerDialog::setupUi(const QString &title) {
    setWindowTitle(QString(tr("View detail resource: %1")).arg(title));
    static constexpr int editorW{640};
    static constexpr int offset{30};
    const int mainH{800};
    this->setMinimumWidth(editorW + offset);
    const int frameH = this->style()->pixelMetric(QStyle::PM_TitleBarHeight) +
                       (this->style()->pixelMetric(QStyle::PM_DefaultFrameWidth) * 2);
    this->resize(editorW, mainH - frameH + offset);

    m_editor = new PlainTextEdit(this);
    m_editor->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_editor->setFont(QFont("JetBrains Mono", UiConst::FONT_SIZE));
    m_editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_editor->setMinimumWidth(editorW);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_editor);

    setLayout(layout);
}

void ResourceViewerDialog::loadContent() {
    const auto textContent = m_viewService.loadTextResource(m_resourceId);
    if (!textContent) {
        m_editor->setPlainText(tr("No content available."));
        m_editor->setReadOnly(true);
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

void ResourceViewerDialog::setupActions() {
    auto* toolbar = new QToolBar(this);
    toolbar->setObjectName("ResourceViewerToolbar");
    toolbar->setMovable(false);

    auto* leftSpacer = new QWidget(toolbar);
    leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(leftSpacer);

    auto* actionSave = toolbar->addAction(tr("Save"));
    actionSave->setIcon(QIcon(":/icons/save.ico"));
    QObject::connect(actionSave, &QAction::triggered, this, [this]() {
        m_originalContent = m_editor->toPlainText();
        m_viewService.saveTextResource(m_resourceId, m_originalContent);
    });

    auto* actionReload = toolbar->addAction(tr("Reload"));
    actionReload->setIcon(QIcon(":/icons/load.ico"));
    QObject::connect(actionReload, &QAction::triggered, this, [this]() {
        const auto text = m_viewService.loadTextResource(m_resourceId);
        if (!text) { return; }

        m_originalContent = *text;
        m_editor->setPlainText(m_originalContent);
    });

    auto* actionToggleSH = toolbar->addAction(tr("Toggle"));
    actionToggleSH->setCheckable(true);
    actionToggleSH->setChecked(m_isAppliedSH);

    QIcon toggleIcon;
    const QPixmap pixmap(":/icons/syntax.ico");
    toggleIcon.addPixmap(pixmap, QIcon::Normal, QIcon::On);
    toggleIcon.addPixmap(toggleIcon.pixmap(pixmap.size(), QIcon::Disabled), QIcon::Normal,
                         QIcon::Off);
    actionToggleSH->setIcon(toggleIcon);

    QObject::connect(actionToggleSH, &QAction::toggled, this, [this, actionToggleSH](bool checked) {
        m_isAppliedSH = checked;
        actionToggleSH->setToolTip(checked ? tr("Disable syntax highlighting")
                                           : tr("Enable syntax highlighting"));

        if (checked) {
            applySyntaxHighlightingTheme();
        } else {
            disableSyntaxHighlightingTheme();
        }
    });

    emit actionToggleSH->toggled(m_isAppliedSH);

    auto* rightSpacer = new QWidget(toolbar);
    rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(rightSpacer);

    // NOLINTNEXTLINE (-Wswitch-default)
    switch (m_type) {
        case ResourceType::plainText:
            m_editor->setReadOnly(false);
            actionSave->setEnabled(true);
            actionReload->setEnabled(true);
            break;
        case ResourceType::cCppCode:
            m_editor->setReadOnly(true);
            actionSave->setEnabled(false);
            actionReload->setEnabled(true);
            break;
        case ResourceType::htmlDoc:
        case ResourceType::pdfDoc:
        case ResourceType::epubDoc:
            m_editor->setReadOnly(true);
            actionSave->setEnabled(false);
            actionReload->setEnabled(false);
            break;
    }

    layout()->setMenuBar(toolbar);
}

void ResourceViewerDialog::applySyntaxHighlightingTheme() {
    if (m_editor == nullptr) { return; }

    // Chọn theme tô màu
    const CppHighlighterTheme hlTheme =
        (m_currentTheme == Theme::light) ? createLightTheme() : createDarkTheme();

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

void ResourceViewerDialog::disableSyntaxHighlightingTheme() {
    if (m_editor == nullptr) { return; }

    if (m_cppHighlighter != nullptr) { m_cppHighlighter->stopGradualRehighlight(); }

    delete m_cppHighlighter;
    m_cppHighlighter = nullptr;
}
