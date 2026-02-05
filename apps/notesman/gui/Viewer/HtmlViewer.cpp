#include <memory>
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
#include "DialogUtils.hpp"

#ifdef Q_OS_WIN
    #include "WebView2Widget.hpp"
    #include "WebView2Guard.hpp"
#elif defined(Q_OS_LINUX)
    #include "WebKitGTKGuard.hpp"
#endif

namespace {
    bool ensureHtmlRuntimeAvailable(QWidget* parent) {
#ifdef Q_OS_WIN
        if (WebView2Guard::instance().available()) { return true; }
        DialogUtils::showError(parent, QObject::tr("Missing Runtime"),
                               QObject::tr("Microsoft WebView2 Runtime is not installed."));
#elif defined(Q_OS_LINUX)
        if (WebKitGTKGuard::instance().available()) { return true; }
        DialogUtils::showError(parent, QObject::tr("Missing Runtime"),
                               QObject::tr("WebKitGTK runtime library is missing."));
#endif
        return false;
    }
} // namespace

HtmlViewer::HtmlViewer(QString title, QWidget* parent) : m_title(std::move(title)) {
#ifdef Q_OS_WIN
    m_rootWidget = new QWidget(parent);
    setupView();
#elif defined(Q_OS_LINUX)
    m_rootWidget = nullptr;
#endif
}

std::unique_ptr<HtmlViewer> HtmlViewer::createFromFile(QString title, QString path,
                                                       QWidget* parent) {
    if (!ensureHtmlRuntimeAvailable(parent)) { return nullptr; }

    auto view = std::unique_ptr<HtmlViewer>(new HtmlViewer(std::move(title), parent));
    view->initFromFile(std::move(path));

    return view;
}

std::unique_ptr<HtmlViewer> HtmlViewer::createFromMemory(QString title, QString html,
                                                         QWidget* parent) {
    if (!ensureHtmlRuntimeAvailable(parent)) { return nullptr; }

    auto view = std::unique_ptr<HtmlViewer>(new HtmlViewer(std::move(title), parent));
    view->initFromMemory(std::move(html));

    return view;
}

std::unique_ptr<HtmlViewer> HtmlViewer::createFromUrl(QString title, QUrl url, QWidget* parent) {
    if (!ensureHtmlRuntimeAvailable(parent)) { return nullptr; }

    auto view = std::unique_ptr<HtmlViewer>(new HtmlViewer(std::move(title), parent));
    view->initFromUrl(std::move(url));

    return view;
}

void HtmlViewer::setupView() {
    auto* layout = new QVBoxLayout(m_rootWidget);
    layout->setContentsMargins(0, 0, 0, 0);

#ifdef Q_OS_WIN
    m_view = new WebView2Widget(m_rootWidget);
    layout->addWidget(m_view);
#endif
}

void HtmlViewer::initFromFile(QString path) {
    if (path.isEmpty()) { return; }

    m_htmlPath = std::move(path);

#ifdef Q_OS_WIN
    if (m_view == nullptr) { return; }
    m_view->loadFile(m_htmlPath);
#elif defined(Q_OS_LINUX)
    m_process = new QProcess(m_rootWidget);

    const QString program = QCoreApplication::applicationDirPath() + "/webkitgtk_viewer";
    const QString uri = QUrl::fromLocalFile(m_htmlPath).toString();
    const QString wTitle = QObject::tr("View detail resource: %1").arg(m_title);

    m_process->start(program, {uri, wTitle});
#endif
}

void HtmlViewer::initFromMemory(QString html) {
    if (html.isEmpty()) { return; }

    m_htmlContent = std::move(html);

#ifdef Q_OS_WIN
    if (m_view == nullptr) { return; }
    m_view->loadHtml(m_htmlContent);
#elif defined(Q_OS_LINUX)
    m_process = new QProcess(m_rootWidget);

    const QString program = QCoreApplication::applicationDirPath() + "/webkitgtk_viewer";
    const QString wTitle = QObject::tr("View detail resource: %1").arg(m_title);

    m_process->start(program, {"--stdin", wTitle});

    if (!m_process->waitForStarted()) { return; }

    m_process->write(m_htmlContent.toUtf8());
    m_process->closeWriteChannel();
#endif
}

void HtmlViewer::initFromUrl(QUrl url) {
    if (!url.isValid()) { return; }

    m_url = std::move(url);

#ifdef Q_OS_WIN
    if (m_view == nullptr) { return; }
    m_view->loadUrl(m_url);
#elif defined(Q_OS_LINUX)
    m_process = new QProcess(m_rootWidget);

    const QString program = QCoreApplication::applicationDirPath() + "/webkitgtk_viewer";
    const QString wTitle = QObject::tr("View detail resource: %1").arg(m_title);
    const QString uri = m_url.toString(QUrl::FullyEncoded);

    m_process->start(program, {uri, wTitle});
#endif
}

QWidget* HtmlViewer::widget() {
#ifdef Q_OS_WIN
    return m_rootWidget;
#elif defined(Q_OS_LINUX)
    return nullptr;
#endif
}

void HtmlViewer::setupToolbar(QToolBar* toolbar) {
    if (toolbar == nullptr) { return; }

#ifdef Q_OS_WIN
    if (m_view == nullptr) { return; }

    if (!supportsSearch()) {
        toolbar->setVisible(false);
        return;
    }

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

bool HtmlViewer::supportsSearch() const {
    return !m_url.isValid();
}
