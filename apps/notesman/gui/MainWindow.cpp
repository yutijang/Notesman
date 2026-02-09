#include <memory>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>
#include <sqlite3.h>
#include <qminmax.h>
#include <QMainWindow>
#include <QTabWidget>
#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QShowEvent>
#include <QScreen>
#include <QMenu>
#include <QPoint>
#include <QTimer>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QProgressDialog>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QDir>
#include <QModelIndexList>
#include <QProcess>
#include <QtTypes>
#include <QKeySequence>
#include <Qt>
#include <QItemSelection>
#include <QString>
#include <QColor>
#include <QFileInfo>
#include <QCoreApplication>
#include <QStringList>
#include <QPushButton>
#include <QOverload>
#include <QLatin1String>

#include "ContentMode.hpp"
#include "HtmlViewer.hpp"
#include "IResourceViewer.hpp"
#include "PdfViewer.hpp"
#include "SettingsData.hpp"
#include "TextViewer.hpp"
#include "UiConstants.hpp"
#include "BrowseTabWidget.hpp"
#include "AddTabWidget.hpp"
#include "SettingsTabWidget.hpp"
#include "MainWindow.hpp"
#include "CodeEditorLineHighlighter.hpp"
#include "ResultsTable.hpp"
#include "cpphighlightertheme.hpp"
#include "cpphighlighter.hpp"
#include "model.hpp"
#include "NotesAppCore.hpp"
#include "PlainTextEdit.hpp"
#include "AppController.hpp"
#include "InfoCornerWidget.hpp"
#include "app_version.hpp"
#include "ResourceSearchWorker.hpp"
#include "UpdateInfoSummary.hpp"
#include "DialogUtils.hpp"
#include "ResourceViewService.hpp"
#include "ResourceViewerDialog.hpp"
#include "SettingsManager.hpp"
#include "Logger.hpp"
#include "MarkdownToHtml.hpp"
#include "EpubResolver.hpp"

#if defined(Q_OS_LINUX)
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>

    #include "AppImageExtractor.hpp"
#elif defined(Q_OS_WIN)
    #include "helper.hpp"
#endif

namespace {
    constexpr int GUI_WIDTH{1200};
    constexpr int GUI_HEIGHT{800};
    constexpr int DL_MAX_PERCENT{100};
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowIcon(QIcon(":/icons/icon.png"));
    setWindowTitle(tr("Notes Manager"));
    resize(GUI_WIDTH, GUI_HEIGHT);

    buildUi();
}

void MainWindow::buildUi() {
    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);

    setupBrowseTab();
    setupAddTab();
    setupSettingsTab();
    setupIconInfo();
    statusBar()->showMessage(tr("Ready"), 0);

    m_tabWidget->setCurrentWidget(m_browseTab);
}

void MainWindow::setupBrowseTab() {
    m_browseTab = new BrowseTabWidget(this);

    QObject::connect(m_browseTab, &BrowseTabWidget::resourceDoubleClicked, this,
                     &MainWindow::viewResource);

    QObject::connect(m_browseTab, &BrowseTabWidget::contextMenuRequested, this,
                     &MainWindow::showContextMenu);

    QObject::connect(m_browseTab, &BrowseTabWidget::statusUpdateRequest, this,
                     &MainWindow::updateStatus);

    QObject::connect(this, &MainWindow::updateColumnWidthsRequest, m_browseTab,
                     &BrowseTabWidget::updateColumnWidths);

    m_resultsTbl = m_browseTab->resultsTable();
    m_deleteResourceAction = new QAction(tr("Delete Resource"), this);
    m_deleteResourceAction->setIcon(QIcon(":/icons/erase.ico"));
    m_deleteResourceAction->setShortcut(QKeySequence::Delete);
    m_deleteResourceAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_deleteResourceAction->setEnabled(false);
    m_resultsTbl->addAction(m_deleteResourceAction);

    QObject::connect(m_deleteResourceAction, &QAction::triggered, this,
                     [this] { handleContextMenuDeleteAction(m_resultsTbl); });

    QObject::connect(m_resultsTbl->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     [this](const QItemSelection &sel, const QItemSelection &) {
                         m_deleteResourceAction->setEnabled(!sel.isEmpty());
                     });

    m_tabWidget->addTab(m_browseTab, QIcon(":/icons/browse_tab.ico"), tr("Browse"));
}

void MainWindow::setupAddTab() {
    m_addTab = new AddTabWidget(this);

    m_tabWidget->addTab(m_addTab, QIcon(":/icons/add_tab.ico"), tr("Add Notes"));
}

void MainWindow::setupSettingsTab() {
    m_settingsTab = new SettingsTabWidget(this);

    QObject::connect(this, &MainWindow::settingsTabShowNotification, m_settingsTab,
                     &SettingsTabWidget::showNotification);

    QObject::connect(this, &MainWindow::settingsStateChangeRequest, m_settingsTab,
                     &SettingsTabWidget::handleSettingsStateChange);

    QObject::connect(m_settingsTab, &SettingsTabWidget::statusUpdateRequest, this,
                     &MainWindow::updateStatus);

    m_tabWidget->addTab(m_settingsTab, QIcon(":/icons/settings_tab.ico"), tr("Settings"));
}

void MainWindow::setupIconInfo() {
    m_infoWidget = new InfoCornerWidget(this);
    m_tabWidget->setCornerWidget(m_infoWidget, Qt::TopRightCorner);

    QObject::connect(m_infoWidget, &InfoCornerWidget::checkUpdateRequested, this,
                     &MainWindow::onCheckUpdateClicked);

    QObject::connect(m_infoWidget, &InfoCornerWidget::aboutRequested, this, &MainWindow::onAbout);
}

void MainWindow::handleSettingsStateChange(UiConst::SettingsMessageState state) {
    m_settingsMessageState = state;

    if (state == UiConst::SettingsMessageState::notChange) {
        Q_EMIT settingsStateChangeRequest(AppController::defaultUiSettings());
    }
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);                // Gọi base trước

    auto &settings = SettingsManager::instance(); // Tạo INI/JSON config

    // Đọc vị trí lưu
    int x = settings.get("window/main_posX", -1).toInt();
    int y = settings.get("window/main_posY", -1).toInt();
    int w = settings.get("window/main_width", width()).toInt();
    int h = settings.get("window/main_height", height()).toInt();

    if (x != -1 && y != -1) {
        // Dùng vị trí lưu (và kích thước nếu cần)
        w = qMax(GUI_WIDTH, w);
        h = qMax(GUI_HEIGHT, h);
        resize(w, h);
        move(x, y);
    } else {
        // Fallback: Căn giữa màn hình
        QScreen* screen = QApplication::primaryScreen();
        QRect geom = screen->geometry();
        move((geom.width() - width()) / 2, (geom.height() - height()) / 2);
    }

    Q_EMIT updateColumnWidthsRequest();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Lưu vị trí và kích thước
    auto &settings = SettingsManager::instance();
    settings.set("window/main_posX", x());
    settings.set("window/main_posY", y());
    settings.set("window/main_width", width());
    settings.set("window/main_height", height());

    QMainWindow::closeEvent(event); // Gọi base để đóng
}

// ===================================================
// Core injection & helpers
// ===================================================
void MainWindow::setCore(NotesAppCore* core) {
    m_core = core;

    m_resourceViewService = std::make_unique<ResourceViewService>(*m_core);

    m_tabWidget->setTabEnabled(m_tabWidget->indexOf(m_addTab), true);
    m_tabWidget->setTabEnabled(m_tabWidget->indexOf(m_browseTab), true);
}

// NOLINTNEXTLINE (bugprone-easily-swappable-parameters)
void MainWindow::viewResource(int id, ResourceType type, const QString &title, const QString &path,
                              const QString &url) {
    std::unique_ptr<IResourceViewer> viewer;

    switch (type) {
        case ResourceType::plainText:
        case ResourceType::cCppCode : {
            const bool editable = (type == ResourceType::plainText && path.isEmpty());
            const UiConst::Theme theme = m_appController->currentTheme();

            viewer = std::make_unique<TextViewer>(static_cast<sqlite3_int64>(id), editable,
                                                  *m_resourceViewService, theme, this);
            break;
        }
        case ResourceType::htmlDoc:
        case ResourceType::markdown: {
            const QString absolutePath = resolveResPath(path);

            if (type == ResourceType::markdown) {
                const QString htmlFileFromMd =
                    MarkdownToHtml::convertFileToHtml(absolutePath, m_appController->isDarkTheme());
                if (htmlFileFromMd.isEmpty()) { return; }
                viewer =
                    HtmlViewer::createFromFile(title, htmlFileFromMd, ContentMode::htmlFile, this);
            } else {
                viewer =
                    HtmlViewer::createFromFile(title, absolutePath, ContentMode::htmlFile, this);
            }

            if (!viewer) {
                Log::err("Invalid WebView2 runtime");
                return;
            }

            break;
        }
        case ResourceType::url: {
            const QUrl qurl = QUrl::fromUserInput(url);
            viewer = HtmlViewer::createFromUrl(title, qurl, ContentMode::url, this);
            if (!viewer) {
                Log::err("Invalid WebView2 runtime");
                return;
            }
            break;
        }
        case ResourceType::pdfDoc: {
            viewer = std::make_unique<PdfViewer>(path, this);
            break;
        }
        case ResourceType::epubDoc: {
            auto epubResolvedPathOtp = EpubResolver::resolveToHtml(path);
            if (epubResolvedPathOtp) {
                viewer = HtmlViewer::createFromFile(title, *epubResolvedPathOtp, ContentMode::epub,
                                                    this);
                if (!viewer) {
                    Log::err("Invalid WebView2 runtime");
                    return;
                }
            }

            break;
        }
        case ResourceType::unknown:
        case ResourceType::count  : break;
    }

    if (!viewer) { return; }
    {
#ifdef Q_OS_LINUX
        if (viewer->usesExternalWindow()) {
            if (m_viewerLocked) { return; }

            m_viewerLocked = true;
            this->setEnabled(false);

            auto* htmlViewer = dynamic_cast<HtmlViewer*>(viewer.get());
            if (htmlViewer == nullptr) {
                m_viewerLocked = false;
                this->setEnabled(true);
                return;
            }

            QProcess* proc = htmlViewer->process();
            if (proc == nullptr) {
                m_viewerLocked = false;
                this->setEnabled(true);
                return;
            }

            m_externalViewer = std::move(viewer);

            QObject::connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                             this, [this]() {
                                 m_externalViewer.reset(); // delete HtmlViewer
                                 m_viewerLocked = false;
                                 this->setEnabled(true);
                             });

            return;
        }
#endif
    }
    auto* dlg = new ResourceViewerDialog{title, std::move(viewer), this};

    dlg->exec();
}

void MainWindow::showContextMenu(const QPoint &pos, int id, ResourceType type, const QString &title,
                                 const QString &path, const QString &url) {
    if (m_browseTab == nullptr) {
        Log::warn("BrowseTabWidget not initialized!");
        return;
    }

    if (m_resultsTbl == nullptr) {
        Log::warn("ResultsTable not available!");
        return;
    }

    QMenu menu(this);

    QAction* viewAction{};
    viewAction = menu.addAction(tr("View Resource"));
    viewAction->setIcon(QIcon(":/icons/view.ico"));
    QObject::connect(viewAction, &QAction::triggered, this, [this, id, type, title, path, url]() {
        viewResource(id, type, title, path, url);
    });

    menu.addSeparator();

    menu.addAction(m_deleteResourceAction);

    menu.exec(m_resultsTbl->viewport()->mapToGlobal(
        pos + QPoint(5, 5))); // NOLINT(readability-magic-numbers)
}

void MainWindow::setAppController(AppController* controller) {
    m_appController = controller;

    QObject::connect(m_appController, &AppController::settingsLoaded, this,
                     [this](const SettingsData &settings) {
                         if (m_tabWidget->currentIndex() == 2) {
                             Q_EMIT settingsUiRefreshRequest(settings);
                         }
                     });

    QObject::connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        constexpr int settingsTabIndex{2};
        if (index == settingsTabIndex && m_appController) {
            const SettingsData ui = m_appController->currentUiSettings();
            Q_EMIT settingsUiRefreshRequest(ui);
        }
    });

    QObject::connect(this, &MainWindow::settingsUiRefreshRequest, m_settingsTab,
                     &SettingsTabWidget::handleUiRefreshRequest);
    QObject::connect(m_browseTab, &BrowseTabWidget::loadAllDataRequested, m_appController,
                     &AppController::handleGetAllDataRequest);
    QObject::connect(m_browseTab, &BrowseTabWidget::loadResourceByTypeRequested, m_appController,
                     &AppController::handleLoadResourceByTypeRequest);
    QObject::connect(m_appController, &AppController::displayResultForGetAll, m_browseTab,
                     &BrowseTabWidget::displayResults);
    QObject::connect(m_settingsTab, &SettingsTabWidget::defaultSettingsRequested, m_appController,
                     &AppController::handleDefaultSettingsRequest);
    QObject::connect(m_appController, &AppController::settingsUpdateStatus, this,
                     [this](const QString &, UiConst::SettingsMessageState state,
                            UiConst::SettingsTabNotiLevel /*unused*/) {
                         this->handleSettingsStateChange(state);
                     });

    QObject::connect(m_appController, &AppController::settingsUpdateStatus, m_settingsTab,
                     &SettingsTabWidget::showNotification);
    QObject::connect(m_appController, &AppController::initialSettingsLoaded, m_settingsTab,
                     &SettingsTabWidget::handleInitialSettingsLoad);
    QObject::connect(m_settingsTab, &SettingsTabWidget::applySettingsRequested, m_appController,
                     &AppController::handleApplySettingsRequest);
    QObject::connect(m_appController, &AppController::requestSyntaxHighlightingUpdate, this,
                     &MainWindow::handleSyntaxHighlightingUpdate);
    QObject::connect(m_addTab, &AddTabWidget::applySyntaxHighlighterRequest, this,
                     &MainWindow::handleSyntaxHighlightingFromAddTabRequested);
    QObject::connect(m_appController, &AppController::addTabNotiRequest, m_addTab,
                     &AddTabWidget::showNotification);
    QObject::connect(m_addTab, &AddTabWidget::addNoteRequested, m_appController,
                     &AppController::handleAddNoteRequest);
    QObject::connect(m_appController, &AppController::resetAddTabInputsRequest, m_addTab,
                     &AddTabWidget::resetAddTabInputs);
    QObject::connect(m_browseTab, &BrowseTabWidget::searchRequested, m_appController,
                     &AppController::handleSearchRequest);
    QObject::connect(m_appController, &AppController::searchFinishedFromController, m_browseTab,
                     &BrowseTabWidget::handleResultsSearchRequested);
    QObject::connect(this, &MainWindow::checkUpdateRequest, m_appController,
                     &AppController::handleCheckUpdateRequested);
    QObject::connect(this, &MainWindow::updateDecision, m_appController,
                     &AppController::onUpdateDecision);
    QObject::connect(m_settingsTab, &SettingsTabWidget::requestGoogleLogin, m_appController,
                     &AppController::handleLoginGMRequested);
    QObject::connect(m_settingsTab, &SettingsTabWidget::requestGoogleUnlink, m_appController,
                     &AppController::handleUnlinkGMRequested);
    QObject::connect(m_appController, &AppController::gmailLinkedForView, m_settingsTab,
                     &SettingsTabWidget::handleAfterLinkAccount);
    QObject::connect(m_appController, &AppController::gmailUnlinked, m_settingsTab,
                     &SettingsTabWidget::handleAfterUnlinkAccount);
    QObject::connect(m_settingsTab, &SettingsTabWidget::requestUpload, m_appController,
                     &AppController::uploadDbAuto);
    QObject::connect(m_settingsTab, &SettingsTabWidget::requestDownload, m_appController,
                     &AppController::downloadDbAuto);
    QObject::connect(this, &MainWindow::startDownloadDBForward, m_settingsTab,
                     &SettingsTabWidget::handleDownloadDBRequested);
    QObject::connect(this, &MainWindow::startUploadDBForward, m_settingsTab,
                     &SettingsTabWidget::handleUploadDBRequested);
    QObject::connect(this, &MainWindow::loginFailedForward, m_settingsTab,
                     &SettingsTabWidget::handleLoginFailed);
    QObject::connect(m_settingsTab, &SettingsTabWidget::cancelLoginRequested, m_appController,
                     &AppController::cancelLoginRequestedForward);
    QObject::connect(m_settingsTab, &SettingsTabWidget::requestDBInfo, m_appController,
                     &AppController::handleGetDBInfoRequested);
    QObject::connect(this, &MainWindow::returnDBInfoForward, m_settingsTab,
                     &SettingsTabWidget::handleDBInfoGot);
    QObject::connect(m_appController, &AppController::deleteDatabaseFileRespondForward,
                     m_settingsTab, &SettingsTabWidget::handleDeleteDBFileRespond);

    QObject::connect(m_settingsTab, &SettingsTabWidget::cleanupEpubCacheNowRequest, m_appController,
                     [this] {
                         auto result = AppController::cleanupOldEpubCacheNow();
                         notiFromCleanupCacheResult(result, UiConst::CleanupMode::epub);
                     });
    QObject::connect(m_settingsTab, &SettingsTabWidget::cleanupMDCacheNowRequest, m_appController,
                     [this] {
                         auto result = AppController::cleanupOldMarkdownCacheNow();
                         notiFromCleanupCacheResult(result, UiConst::CleanupMode::markdown);
                     });
    QObject::connect(this, &MainWindow::onCleanupFinished, m_settingsTab,
                     &SettingsTabWidget::handleButtonAfterCleanup);
}

void MainWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) { retranslateUi(); }
    QMainWindow::changeEvent(event);
}

void MainWindow::retranslateUi() {
    setWindowTitle(tr("Notes Manager"));

    m_tabWidget->setTabText(0, tr("Browse"));
    m_tabWidget->setTabText(1, tr("Add Notes"));
    m_tabWidget->setTabText(2, tr("Settings"));

    // ========= Browse Tab =========
    m_browseTab->retranslateUi();
    // ==============================

    // ========= Add Tab =========
    m_addTab->retranslateUi();
    // ===========================

    // ========= Settings Tab =========
    m_settingsTab->retranslateUi();

    // ========= InfoCornerWidget =========
    m_infoWidget->retranslateUi();

    // ========= AppController =========
    m_appController->updateTranslatedStrings();

    // Dịch lại thông báo đang hiển thị (nếu còn hiệu lực)
    switch (m_settingsMessageState) {
        case UiConst::SettingsMessageState::updated:
            // m_settingsTab->notificationLabel()->setText(tr("Settings updated!"));
            Q_EMIT settingsTabShowNotification(tr("Settings updated!"));
            break;
        case UiConst::SettingsMessageState::notChange:
            // m_settingsTab->notificationLabel()->setText(tr("Settings default!"));
            Q_EMIT settingsTabShowNotification(tr("Settings default!"));
            break;
        case UiConst::SettingsMessageState::none: break;
    }
    // =================================
}

void MainWindow::applySyntaxHighlightingTheme(UiConst::Theme theme) {
    if (m_addTab->textEdit() == nullptr) { return; }

    // Chọn theme tô màu
    const CppHighlighterTheme hlTheme =
        (theme == UiConst::Theme::light) ? createLightTheme() : createDarkTheme();

    // Nếu chưa có highlighter thì tạo mới
    if (m_cppHighlighter == nullptr) {
        m_cppHighlighter = new CppHighlighter(m_addTab->textEdit()->document(), hlTheme);
    } else {
        m_cppHighlighter->stopGradualRehighlight();
        m_cppHighlighter->setTheme(hlTheme);
    }

    // Áp dụng highlight dần (mượt, không đơ)
    m_cppHighlighter->rehighlightGradually(m_addTab->textEdit()->document(),
                                           20, // NOLINT(readability-magic-numbers)
                                           4);

    // Cập nhật dòng caret highlight
    if (m_lineHighlighter != nullptr) {
        delete m_lineHighlighter;
        m_lineHighlighter = nullptr;
    }

    m_lineHighlighter = new CodeEditorLineHighlighter(m_addTab->textEdit());
    if (theme == UiConst::Theme::light) {
        m_lineHighlighter->setColors(QColor("#dBdBdB"), QColor("#efefef"));
    } else {
        m_lineHighlighter->setColors(QColor("#2f2f2f"), QColor("#2a2a2a"));
    }
}

// Position validated
void MainWindow::onAbout() {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("About"));
    msgBox.setTextFormat(Qt::RichText);

    msgBox.setText(tr("%1<br>Version: %2<br>Author: %3<br>Website: <a href=\"%4\" "
                      "style=\"text-decoration: underline;\">%4</a>")
                       .arg(app::meta::NAME)
                       .arg(app::meta::VERSION)
                       .arg(app::meta::AUTHOR)
                       .arg(app::meta::WEBSITE));

    auto* msgLabel = msgBox.findChild<QLabel*>("qt_msgbox_label");
    if (msgLabel != nullptr) { msgLabel->setStyleSheet("padding: 10px 30px 30px 10px;"); }

    msgBox.setStandardButtons(QMessageBox::Ok);

    QPushButton* aboutQtButton = msgBox.addButton(tr("About Qt"), QMessageBox::ActionRole);
    QObject::connect(aboutQtButton, &QPushButton::clicked, this,
                     [this]() { QMessageBox::aboutQt(this); });

    msgBox.ensurePolished();
    msgBox.adjustSize();

    QWidget* root = window();
    if (root == nullptr) {
        msgBox.exec();
        return;
    }

    const QRect dialogFrame = msgBox.frameGeometry();
    const QRect parentFrame = root->frameGeometry();

    const int x = parentFrame.center().x() - (dialogFrame.width() / 2);
    const int y = parentFrame.center().y() - (dialogFrame.height() / 2);

    msgBox.move(x, y);

    msgBox.exec();
}

void MainWindow::onCheckUpdateClicked() {
    Q_EMIT checkUpdateRequest();
}

void MainWindow::onUpdateAvailable(const UpdateInfoSummary &infoSummary) {
    if (!infoSummary.isValid()) {
        Q_EMIT updateDecision(false, infoSummary);
        return;
    }

    const auto &relName = infoSummary.releaseName;
    const auto vPos = relName.indexOf('v');
    const auto newVer =
        (vPos >= 0 && vPos + 1 < relName.size()) ? relName.mid(vPos + 1) : QString{};

    const auto reply =
        DialogUtils::showQuestion(this, tr("Update available"),
                                  tr("A new version is available.\n\nCurrent version: %1\nNewer "
                                     "version: %2\n\nDo you want to download it?")
                                      .arg(app::meta::VERSION)
                                      .arg(newVer));

    Q_EMIT updateDecision(reply == QMessageBox::Yes, infoSummary);
}

void MainWindow::onNoUpdateAvailable() {
    DialogUtils::showInfo(this, tr("No update"), tr("You are using the latest version."));
}

void MainWindow::onUpdateCheckFailed(const QString &error) {
    DialogUtils::showWarning(this, tr("Update check failed"), error);
}

void MainWindow::onDownloadStarted() {
    if (m_progressDialog == nullptr) {
        m_progressDialog =
            new QProgressDialog(tr("Downloading update..."), QString(), 0, DL_MAX_PERCENT, this);

        Qt::WindowFlags flags = Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint;
#ifdef Q_OS_WIN
        flags |= Qt::MSWindowsFixedSizeDialogHint;
#endif
        m_progressDialog->setWindowFlags(flags);
        m_progressDialog->setWindowTitle(tr("Updater"));

        m_progressDialog->setWindowModality(Qt::WindowModal);
        m_progressDialog->setAutoClose(true);
        m_progressDialog->setAutoReset(true);
        m_progressDialog->setFixedWidth(350); // NOLINT(readability-magic-numbers)
    }
    m_progressDialog->setValue(0);
    m_progressDialog->show();

    QTimer::singleShot(0, this, [this] {
        if (!m_progressDialog) { return; }

        QWidget* root = this->window();
        if (!root) { return; }

        const QRect parentFrame = root->frameGeometry();
        const QRect dlgFrame = m_progressDialog->frameGeometry();

        const int x = parentFrame.center().x() - (dlgFrame.width() / 2);
        const int y = parentFrame.center().y() - (dlgFrame.height() / 2);

        m_progressDialog->move(x, y);
    });
}

void MainWindow::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if ((m_progressDialog != nullptr) && bytesTotal > 0) {
        const int kpercent = static_cast<int>((bytesReceived * 100) / bytesTotal);
        m_progressDialog->setValue(kpercent);
    }
}

void MainWindow::onDownloadFinished(const QString &filePath) {
    if (m_progressDialog != nullptr) {
        m_progressDialog->setValue(DL_MAX_PERCENT);
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }

    if (m_appController == nullptr) {
        Q_EMIT onDownloadFailed(tr("Internal error: no controller"));
        return;
    }

    QFileInfo file(filePath);
    if (!file.exists() || !file.isFile()) {
        Q_EMIT onDownloadFailed(tr("Downloaded file missing"));
        return;
    }

    auto downloadedFileHash = NotesAppCore::computeFileHash(filePath.toUtf8().toStdString());
    if (downloadedFileHash.empty()) {
        Q_EMIT onDownloadFailed(tr("Cannot compute file hash"));
        return;
    }

    const auto assetHash = m_appController->lastUpdateInfoAssetHash();
    if (assetHash.isEmpty()) {
        Q_EMIT onDownloadFailed(tr("Invalid expected hash"));
        return;
    }

    if (QString::fromStdString(downloadedFileHash) != assetHash) {
        Q_EMIT onDownloadFailed(tr("Hash mismatch"));
        return;
    }

    const auto reply = DialogUtils::showQuestion(
        this, tr("Download complete"),
        tr("The update package has been downloaded:\n%1\n\nDo you want update?").arg(filePath));
    if (reply == QMessageBox::Yes) { runUpdate(filePath); }
}

void MainWindow::handleDownloadFailCauseTimeout() {
    if (m_progressDialog != nullptr) {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }
}

#if defined(Q_OS_WIN)
void MainWindow::handleWindowsUpdate(const QString &filePath) {
    const QString targetDir = QCoreApplication::applicationDirPath();
    const auto kUpdaterName = QStringLiteral("Updater.exe");
    const QString updaterPath = targetDir + "/" + kUpdaterName;

    if (!QFile::exists(updaterPath)) {
        Log::err("Missing {}. Update failed!", kUpdaterName.toStdString());
        DialogUtils::showError(this, tr("Error"),
                               tr("Missing %1. Update failed!").arg(kUpdaterName));
        return;
    }

    const auto currentPID = QString::number(QCoreApplication::applicationPid());

    const auto resDirStd = Utils::getDirectoryOrFileName(m_appController->resourceDir());
    const QString resourceDirName =
        resDirStd.empty() ? "NULL_OR_ROOT" : QString::fromStdString(resDirStd);

    QStringList args;
    args << "--stage1";
    args << currentPID;
    args << targetDir;
    args << filePath;
    args << resourceDirName;

    QProcess::startDetached(updaterPath, args);

    qApp->quit();
}
#elif defined(Q_OS_LINUX)
void MainWindow::handleLinuxUpdate(const QString &filePath) {
    QString currentAppImage = qEnvironmentVariable("APPIMAGE");
    if (currentAppImage.isEmpty()) { currentAppImage = QCoreApplication::arguments().first(); }

    const QString &downloadedAppImage = filePath;

    const QString updaterTmpPath = "/tmp/notesman-updater";
    QFile::remove(updaterTmpPath);

    if (!AppImageExtractor::extractUpdater(downloadedAppImage, updaterTmpPath) ||
        !QFile::exists(updaterTmpPath)) {
        Log::err("Cannot extract updater from AppImage. Update failed!");
        DialogUtils::showError(this, tr("Error"),
                               tr("Cannot extract updater from AppImage. Update failed!"));
        return;
    }

    {
        const QByteArray pathUtf8 = updaterTmpPath.toLocal8Bit();
        ::chmod(pathUtf8.constData(),
                S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH); // 0755
    }

    QStringList args{currentAppImage, downloadedAppImage};
    const QString workDir = QDir::tempPath();

    pid_t pid = fork();
    if (pid == -1) {
        Log::err("Cannot start updater process. Update failed!");
        DialogUtils::showError(this, tr("Error"),
                               tr("Cannot start updater process. Update failed!"));
        return;
    }

    if (pid == 0) {
        // CHILD: detach, set working dir, safe argv, then exec
        ::setsid(); // detach from controlling terminal

        // chdir to safe work dir
        ::chdir(workDir.toLocal8Bit().constData());

        // Prepare stable C strings for exec
        QByteArray upBA = updaterTmpPath.toLocal8Bit();
        QByteArray a0BA = currentAppImage.toLocal8Bit();
        QByteArray a1BA = downloadedAppImage.toLocal8Bit();

        // strdup so pointers still valid even if Qt objects go away (not necessary after fork, but
        // harmless)
        char* upC = strdup(upBA.constData());
        char* a0C = strdup(a0BA.constData());
        char* a1C = strdup(a1BA.constData());

        char* argvExec[] = {upC, a0C, a1C, nullptr};

        // Optional: close inherited fds (File Descriptors (FD))
        // except stdin/out/err (helps avoid fd leaks)
        long maxFks = sysconf(_SC_OPEN_MAX);
        for (int fd = 3; fd < maxFks; ++fd) { ::close(fd); }

        // Exec: if returns, it failed — write errno to logfile for debug
        ::execv(upC, argvExec);

        // exec failed -> log and exit
        int err = errno;
        int fd = ::open("/tmp/notesman-updater.err", O_WRONLY | O_CREAT | O_TRUNC,
                        0644); // NOLINT(readability-magic-numbers)
        if (fd != -1) {
            const char* msg = "execv failed: ";
            ::write(fd, msg, strlen(msg));
            const char* estr = strerror(err);
            ::write(fd, estr, strlen(estr));
            ::write(fd, "\n", 1);
            ::close(fd);
        }

        free(upC);
        free(a0C);
        free(a1C);

        _exit(127);                                         // NOLINT(readability-magic-numbers)
    }

    QTimer::singleShot(200, qApp, &QCoreApplication::quit); // NOLINT(readability-magic-numbers)
}
#endif

void MainWindow::runUpdate(const QString &filePath) {
#if defined(Q_OS_WIN)
    handleWindowsUpdate(filePath);
#elif defined(Q_OS_LINUX)
    handleLinuxUpdate(filePath);
#endif
}

void MainWindow::onDownloadFailed(const QString &errorString) {
    DialogUtils::showWarning(this, tr("Download failed"), errorString);
}

void MainWindow::updateStatus(const QString &message, int timeout) {
    statusBar()->showMessage(message, timeout);
}

void MainWindow::handleSyntaxHighlightingUpdate(UiConst::Theme theme) {
    applySyntaxHighlightingTheme(theme);
}

void MainWindow::handleSyntaxHighlightingFromAddTabRequested(bool checked) {
    if (checked) {
        const UiConst::Theme curTheme = m_appController->currentTheme();
        applySyntaxHighlightingTheme(curTheme);
    } else {
        disableSyntaxHighlightingTheme();
    }
}

void MainWindow::disableSyntaxHighlightingTheme() {
    if (m_addTab->textEdit() == nullptr) { return; }

    if (m_cppHighlighter != nullptr) { m_cppHighlighter->stopGradualRehighlight(); }

    delete m_cppHighlighter;
    m_cppHighlighter = nullptr;
}

// --- BEGIN showContextMenu helper ---

void MainWindow::handleContextMenuDeleteAction(ResultsTable* resultTable) {
    if (resultTable == nullptr) { return; }

    const auto selectedRows = resultTable->selectionModel()->selectedRows();
    if (selectedRows.empty()) { return; }

    std::vector<sqlite3_int64> idsToDelete;
    const auto idsToDelCount =
        static_cast<std::vector<sqlite3_int64>::size_type>(selectedRows.size());
    idsToDelete.reserve(idsToDelCount);

    QString textSel;
    if (idsToDelCount == 1) {
        const auto &index = selectedRows[0];
        auto* itemSel = resultTable->item(index.row(), 1);
        if (itemSel != nullptr) { textSel = itemSel->text(); }
    }

    for (const QModelIndex &idx : selectedRows) {
        const sqlite3_int64 id = extractIdFromRow(resultTable, idx.row());
        if (id > 0) { idsToDelete.push_back(id); }
    }

    removeSelectedRowsFromTable(resultTable, selectedRows);

    m_core->deleteResources(idsToDelete);

    if (idsToDelCount > 1) {
        updateStatus(tr("Deleted %1 resources").arg(idsToDelCount), UiConst::NOTI_TIMEOUT);
    } else if (!textSel.isEmpty()) {
        updateStatus(tr("Deleted %1").arg(textSel), UiConst::NOTI_TIMEOUT);
    } else {
        updateStatus(tr("Deleted resource"), UiConst::NOTI_TIMEOUT);
    }
}

sqlite3_int64 MainWindow::extractIdFromRow(ResultsTable* resultTable, int row) {
    auto* item = resultTable->item(row, 1);
    if (item == nullptr) { return -1; }
    return item->data(static_cast<int>(ResultsTable::ItemRole::resourceId)).toLongLong();
}

std::optional<ResourceType> MainWindow::extractTypeFromRow(ResultsTable* resultTable, int row) {
    auto* item = resultTable->item(row, 1);
    if (item == nullptr) { return std::nullopt; }

    const QVariant vRes = item->data(static_cast<int>(ResultsTable::ItemRole::resourceType));
    bool ok{};
    const int raw = vRes.toInt(&ok);
    if (!ok) { return std::nullopt; }

    return static_cast<ResourceType>(raw);
}

void MainWindow::removeSelectedRowsFromTable(ResultsTable* table,
                                             const QModelIndexList &selectedRows) {
    table->setUpdatesEnabled(false);
    table->setSortingEnabled(false);

    for (const auto &selectedRow : std::ranges::reverse_view(selectedRows)) {
        table->removeRow(selectedRow.row());
    }

    table->setUpdatesEnabled(true);
    table->setSortingEnabled(true);

    table->clearSelection();
    table->setCurrentIndex(QModelIndex());
    table->clearFocus();
    table->viewport()->update();
}

// --- END showContextMenu helper ---

QString MainWindow::resolveResPath(const QString &path) {
    QString absolutePath;

    QFileInfo fi(path);

    if (fi.isAbsolute()) {
        absolutePath = fi.absoluteFilePath();
    } else {
        QDir baseDir(
            QString::fromStdString(m_appController->resourceDir().lexically_normal().string()));

        QString relativePath = path;

        if (relativePath.startsWith("resources/") || relativePath.startsWith("resources\\")) {
            relativePath = relativePath.mid(QString("resources/").length());
        }

        absolutePath = baseDir.absoluteFilePath(relativePath);
    }

    return absolutePath;
}

void MainWindow::notiFromCleanupCacheResult(UiConst::CleanupResult result,
                                            UiConst::CleanupMode mode) {
    QString typeText =
        (mode == UiConst::CleanupMode::epub) ? QStringLiteral("EPUB") : QStringLiteral("Markdown");
    switch (result) {
        case UiConst::CleanupResult::pathError: {
            Q_EMIT settingsTabShowNotification(
                tr("%1 cache directory not found or inaccessible.").arg(typeText));
            break;
        }
        case UiConst::CleanupResult::alreadyEmpty: {
            Q_EMIT settingsTabShowNotification(
                tr("The %1 cache folder is already empty. No action required.").arg(typeText));
            break;
        }
        case UiConst::CleanupResult::success: {
            Q_EMIT settingsTabShowNotification(
                tr("All temporary %1 cache files have been successfully cleared.").arg(typeText));
            break;
        }
    }

    Q_EMIT onCleanupFinished(mode);
}
