#include <utility>
#include <QString>
#include <QVBoxLayout>
#include <QObject>
#include <Qt>
#include <QToolBar>
#include <QInputDialog>
#include <QLineEdit>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBrowser>
#include <QDesktopServices>
#include <QUrl>

#include "HtmlViewer.hpp"

#ifdef Q_OS_WIN
    #include "WebView2Widget.hpp"
#endif

HtmlViewer::HtmlViewer(QString path, QWidget* parent) : m_htmlPath(std::move(path)) {
    m_rootWidget = new QWidget(parent);

    setupView();
    loadFile();
}

/*
void HtmlViewer::setupView() {
    m_browser = new QTextBrowser(m_rootWidget);
    m_browser->setOpenExternalLinks(true);
    m_browser->setOpenLinks(true);

    auto* layout = new QVBoxLayout(m_rootWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_browser);
}
*/
void HtmlViewer::setupView() {
    auto* layout = new QVBoxLayout(m_rootWidget);
    layout->setContentsMargins(0, 0, 0, 0);

#ifdef Q_OS_WIN
    m_view = new WebView2Widget(m_rootWidget);
    layout->addWidget(m_view);
#endif
}

void HtmlViewer::loadFile() {
    /*
    if (m_htmlPath.isEmpty()) {
        m_browser->setHtml(QObject::tr("<p><i>No HTML file</i></p>"));
        return;
    }

    const QUrl url = QUrl::fromLocalFile(m_htmlPath);
    m_browser->setSource(url);
*/
#ifdef Q_OS_WIN
    if (!m_htmlPath.isEmpty()) { m_view->loadFile(m_htmlPath); }
#endif
}

QWidget* HtmlViewer::widget() {
    return m_rootWidget;
}

/*
void HtmlViewer::setupToolbar(QToolBar* toolbar) {
    if (toolbar == nullptr) { return; }

    auto* actionSearch = toolbar->addAction(QObject::tr("Search"));
    actionSearch->setIcon(QIcon(":/icons/search.ico"));
    actionSearch->setShortcut(QKeySequence::Find);
    QObject::connect(actionSearch, &QAction::triggered, toolbar, [this]() { startSearch(); });

    // Open in external browser
    auto* actionOpenExternal = toolbar->addAction(QObject::tr("Open in Browser"));
    actionOpenExternal->setIcon(QIcon(":/icons/external_browser.ico"));
    QObject::connect(actionOpenExternal, &QAction::triggered, toolbar, [this]() {
        if (!m_htmlPath.isEmpty()) { QDesktopServices::openUrl(QUrl::fromLocalFile(m_htmlPath)); }
    });

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
*/

void HtmlViewer::setupToolbar(QToolBar* toolbar) {
    if (toolbar == nullptr) { return; }

    if (m_view == nullptr) { return; }

    auto* actSearch = toolbar->addAction(QObject::tr("Search"));
    actSearch->setShortcut(QKeySequence::Find);
    QObject::connect(actSearch, &QAction::triggered, toolbar, [this]() {
        bool ok{};
        const QString text =
            QInputDialog::getText(m_rootWidget, QObject::tr("Search"), QObject::tr("Find:"),
                                  QLineEdit::Normal, m_lastSearchText, &ok);
        if (!ok || text.isEmpty()) { return; }
        m_lastSearchText = text;
        m_view->find(text, false);
    });

    auto* actNext = new QAction(m_rootWidget);
    actNext->setShortcut(Qt::Key_F3);
    QObject::connect(actNext, &QAction::triggered, m_rootWidget,
                     [this]() { m_view->find(m_lastSearchText, false); });

    auto* actPrev = new QAction(m_rootWidget);
    actPrev->setShortcut(Qt::SHIFT | Qt::Key_F3);
    QObject::connect(actPrev, &QAction::triggered, m_rootWidget,
                     [this]() { m_view->find(m_lastSearchText, true); });

    m_rootWidget->addAction(actNext);
    m_rootWidget->addAction(actPrev);
}

void HtmlViewer::startSearch() {
    bool ok{};
    const QString text =
        QInputDialog::getText(m_rootWidget, QObject::tr("Search"), QObject::tr("Find:"),
                              QLineEdit::Normal, m_lastSearchText, &ok);

    if (!ok || text.isEmpty()) { return; }

    m_lastSearchText = text;
    findNext();
}

void HtmlViewer::findNext() {
    //     if (m_lastSearchText.isEmpty()) { return; }

    //     if (!m_browser->find(m_lastSearchText)) {
    //         // wrap around
    //         QTextCursor c = m_browser->textCursor();
    //         c.movePosition(QTextCursor::Start);
    //         m_browser->setTextCursor(c);
    //         m_browser->find(m_lastSearchText);
    //     }
}

void HtmlViewer::findPrevious() {
    //     if (m_lastSearchText.isEmpty()) { return; }

    //     if (!m_browser->find(m_lastSearchText, QTextDocument::FindBackward)) {
    //         QTextCursor c = m_browser->textCursor();
    //         c.movePosition(QTextCursor::End);
    //         m_browser->setTextCursor(c);
    //         m_browser->find(m_lastSearchText, QTextDocument::FindBackward);
    //     }
}

bool HtmlViewer::onClose([[maybe_unused]] QWidget* parent) {
    return true;
}
