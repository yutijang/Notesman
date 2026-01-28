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

HtmlViewer::HtmlViewer(QString title, QString path, QWidget* parent)
    : m_htmlPath(std::move(path)), m_title(std::move(title)) {
#ifdef Q_OS_WIN
    m_rootWidget = new QWidget(parent);
#else
    m_rootWidget = nullptr;
#endif

#ifdef Q_OS_WIN
    setupView();
    loadFile();
#elif defined(Q_OS_LINUX)
    loadFile(); // chỉ launch process
#endif
}

HtmlViewer::HtmlViewer(QString title, QString htmlContent, bool fromMemory, QWidget* parent)
    : m_htmlContent(std::move(htmlContent)), m_title(std::move(title)), m_fromMemory(fromMemory) {
    m_rootWidget = new QWidget(parent);
    setupView();
    loadFromMemory();
}

void HtmlViewer::setupView() {
    auto* layout = new QVBoxLayout(m_rootWidget);
    layout->setContentsMargins(0, 0, 0, 0);

#ifdef Q_OS_WIN
    m_view = new WebView2Widget(m_rootWidget);
    layout->addWidget(m_view);
#endif
}

void HtmlViewer::loadFile() {
#ifdef Q_OS_WIN
    if (!m_htmlPath.isEmpty()) { m_view->loadFile(m_htmlPath); }
#endif

#ifdef Q_OS_LINUX
    if (m_htmlPath.isEmpty()) { return; }

    m_process = new QProcess(m_rootWidget);

    const QString program = QCoreApplication::applicationDirPath() + "/webkitgtk_viewer";

    const QString uri = QUrl::fromLocalFile(m_htmlPath).toString();
    const QString wTitle = QObject::tr("View detail resource: %1").arg(m_title);

    m_process->start(program, {uri, wTitle});
#endif
}

void HtmlViewer::loadFromMemory() {
#ifdef Q_OS_WIN
    if (!m_htmlContent.isEmpty()) { m_view->loadHtml(m_htmlContent); }
#endif

#ifdef Q_OS_LINUX
    if (m_htmlContent.isEmpty()) { return; }

    m_process = new QProcess(m_rootWidget);

    const QString program = QCoreApplication::applicationDirPath() + "/webkitgtk_viewer";

    const QString wTitle = QObject::tr("View detail resource: %1").arg(m_title);

    m_process->start(program, {"--stdin", wTitle});

    if (!m_process->waitForStarted()) { return; }

    m_process->write(m_htmlContent.toUtf8());
    m_process->closeWriteChannel();
#endif
}

QWidget* HtmlViewer::widget() {
#ifdef Q_OS_LINUX
    return nullptr;
#else
    return m_rootWidget;
#endif
}

void HtmlViewer::setupToolbar(QToolBar* toolbar) {
    if (toolbar == nullptr) { return; }

#ifdef Q_OS_WIN
    if (m_view == nullptr) { return; }

    auto* actSearch = toolbar->addAction(QObject::tr("Search"));
    actSearch->setIcon(QIcon(":/icons/search.ico"));
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
#endif
}

void HtmlViewer::startSearch() {
#ifdef Q_OS_WIN
// HtmlViewer không chịu trách nhiệm search
// Mọi thứ delegate cho WebView2Widget
#endif
}

void HtmlViewer::findNext() {
#ifdef Q_OS_WIN
// HtmlViewer không chịu trách nhiệm search
// Mọi thứ delegate cho WebView2Widget
#endif
}

void HtmlViewer::findPrevious() {
#ifdef Q_OS_WIN
// HtmlViewer không chịu trách nhiệm search
// Mọi thứ delegate cho WebView2Widget
#endif
}

bool HtmlViewer::onClose([[maybe_unused]] QWidget* parent) {
#ifdef Q_OS_LINUX
    if (m_process != nullptr) {
        m_process->terminate();
        m_process->waitForFinished(500); // NOLINT(readability-magic-numbers)
        delete m_process;
        m_process = nullptr;
    }
#endif

    return true;
}
