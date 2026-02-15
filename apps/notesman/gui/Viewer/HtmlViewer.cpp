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
#include <QFileInfo>

#include "HtmlViewer.hpp"
#include "ContentMode.hpp"
#include "DialogUtils.hpp"
#include "Logger.hpp"

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
                                                       ContentMode mode, QWidget* parent) {
    if (!ensureHtmlRuntimeAvailable(parent)) { return nullptr; }

    auto view = std::unique_ptr<HtmlViewer>(new HtmlViewer(std::move(title), parent));
    view->initFromFile(std::move(path), mode);

    return view;
}

std::unique_ptr<HtmlViewer> HtmlViewer::createFromUrl(QString title, QUrl url, ContentMode mode,
                                                      QWidget* parent) {
    if (!ensureHtmlRuntimeAvailable(parent)) { return nullptr; }

    auto view = std::unique_ptr<HtmlViewer>(new HtmlViewer(std::move(title), parent));
    view->initFromUrl(std::move(url), mode);

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

void HtmlViewer::initFromFile(QString path, ContentMode mode) {
    if (path.isEmpty()) { return; }

    m_htmlPath = std::move(path);

#ifdef Q_OS_WIN
    if (m_view == nullptr) {
        Log::fatal("WebView2Widget failed to initialize.");
        return;
    }

    QFileInfo fi(m_htmlPath);
    if (!fi.exists() || !fi.isFile()) {
        Log::warn("HtmlViewer open: path={}, exists={}, isFile={}",
                  fi.absoluteFilePath().toStdString(), fi.exists(), fi.isFile());
        return;
    }

    QUrl const base = QUrl::fromLocalFile(fi.absolutePath() + "/");

    m_view->setContentMode(mode, base);
    m_view->loadFile(m_htmlPath);
#elif defined(Q_OS_LINUX)
    m_process = new QProcess(m_rootWidget);

    QString const program = QCoreApplication::applicationDirPath() + "/webkitgtk_viewer";
    QString const uri = QUrl::fromLocalFile(m_htmlPath).toString();
    QString const wTitle = QObject::tr("View detail resource: %1").arg(m_title);

    m_process->start(program, {uri, wTitle});
#endif
}

void HtmlViewer::initFromUrl(QUrl url, ContentMode mode) {
    if (!url.isValid()) { return; }

    m_url = std::move(url);

#ifdef Q_OS_WIN
    if (m_view == nullptr) {
        Log::fatal("WebView2Widget failed to initialize.");
        return;
    }

    QUrl base = m_url;
    base.setPath(QString());
    base.setQuery(QString());
    base.setFragment(QString());

    m_view->setContentMode(mode, base);
    m_view->loadUrl(m_url);
#elif defined(Q_OS_LINUX)
    m_process = new QProcess(m_rootWidget);

    QString const program = QCoreApplication::applicationDirPath() + "/webkitgtk_viewer";
    QString const wTitle = QObject::tr("View detail resource: %1").arg(m_title);
    QString const uri = m_url.toString(QUrl::FullyEncoded);

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
        QString const text =
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
