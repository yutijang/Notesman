#include "HtmlViewer.hpp"

#include "ContentMode.hpp"
#include "DialogUtils.hpp"
#include "Logger.hpp"

#include <QDesktopServices>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QObject>
#include <QString>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <Qt>
#include <memory>
#include <utility>

#ifdef Q_OS_WIN
#include "WebView2Guard.hpp"
#include "WebView2Widget.hpp"
#elif defined(Q_OS_LINUX)
#include "WebKitGTKGuard.hpp"

#include <QCoreApplication>
#include <QProcess>
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

    constexpr int K_WAITSTART{3000};
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
    if (!ensureHtmlRuntimeAvailable(parent)) [[unlikely]] { return nullptr; }

    auto view = std::unique_ptr<HtmlViewer>(new HtmlViewer(std::move(title), parent));

    if (!view->initFromFile(std::move(path), mode)) [[unlikely]] { return nullptr; }

    return view;
}

std::unique_ptr<HtmlViewer> HtmlViewer::createFromUrl(QString title, QUrl url, ContentMode mode,
                                                      QWidget* parent) {
    if (!ensureHtmlRuntimeAvailable(parent)) [[unlikely]] { return nullptr; }

    auto view = std::unique_ptr<HtmlViewer>(new HtmlViewer(std::move(title), parent));

    if (!view->initFromUrl(std::move(url), mode)) [[unlikely]] { return nullptr; }

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

bool HtmlViewer::initFromFile(QString path, ContentMode mode) {
    if (path.isEmpty()) [[unlikely]] { return false; }

    m_htmlPath = std::move(path);

#ifdef Q_OS_WIN
    if (m_view == nullptr) {
        Log::fatal("WebView2Widget failed to initialize.");
        return false;
    }

    QFileInfo fi(m_htmlPath);
    if (!fi.exists() || !fi.isFile()) {
        Log::warn("HtmlViewer open: path={}, exists={}, isFile={}",
                  fi.absoluteFilePath().toStdString(), fi.exists(), fi.isFile());
        return false;
    }

    QUrl const base = QUrl::fromLocalFile(fi.absolutePath() + "/");

    m_view->setContentMode(mode, base);
    m_view->loadFile(m_htmlPath);

    return true;
#elif defined(Q_OS_LINUX)
    auto process = std::make_unique<QProcess>(m_rootWidget);

    QString const program = QCoreApplication::applicationDirPath() + "/webkitgtk_viewer";
    QString const uri = QUrl::fromLocalFile(m_htmlPath).toString();
    QString const wTitle = QObject::tr("View detail resource: %1").arg(m_title);

    process->start(program, {uri, wTitle});

    if (!process->waitForStarted(K_WAITSTART)) [[unlikely]] {
        Log::fatal("WebKitGTK process failed to initialize. Error: {}",
                   process->errorString().toStdString());
        return false;
    }

    if (m_process != nullptr) { m_process->deleteLater(); }

    m_process = process.release();

    return true;
#endif
}

bool HtmlViewer::initFromUrl(QUrl url, ContentMode mode) {
    if (!url.isValid()) [[unlikely]] {
        Log::fatal("url INVALID - {}", url.toString().toStdString());
        return false;
    }

    m_url = std::move(url);

#ifdef Q_OS_WIN
    if (m_view == nullptr) [[unlikely]] {
        Log::fatal("WebView2Widget failed to initialize.");
        return false;
    }

    QUrl base = m_url;
    base.setPath(QString());
    base.setQuery(QString());
    base.setFragment(QString());

    m_view->setContentMode(mode, base);
    m_view->loadUrl(m_url);

    return true;
#elif defined(Q_OS_LINUX)
    auto process = std::make_unique<QProcess>(m_rootWidget);

    QString const program = QCoreApplication::applicationDirPath() + "/webkitgtk_viewer";
    QString const wTitle = QObject::tr("View detail resource: %1").arg(m_title);
    QString const uri = m_url.toString(QUrl::FullyEncoded);

    process->start(program, {uri, wTitle});
    if (!process->waitForStarted(K_WAITSTART)) [[unlikely]] {
        Log::fatal("WebKitGTK process failed to initialize. Error: {}",
                   process->errorString().toStdString());
        return false;
    }

    if (m_process != nullptr) { m_process->deleteLater(); }

    m_process = process.release();

    return true;
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
