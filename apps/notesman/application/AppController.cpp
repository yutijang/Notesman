#include "AppController.hpp"

#include "AppSettings.hpp"
#include "CorePaths.hpp"
#include "DialogUtils.hpp"
#include "DownloadManager.hpp"
#include "FileAssociation.hpp"
#include "GoogleDriveService.hpp"
#include "Logger.hpp"
#include "MainWindow.hpp"
#include "NotesAppCore.hpp"
#include "OAuthManager.hpp"
#include "ResourceSearchWorker.hpp"
#include "SettingsData.hpp"
#include "UiConstants.hpp"
#include "UpdateInfoSummary.hpp"
#include "UpdateManager.hpp"
#include "helper.hpp"
#include "model.hpp"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QStyleFactory>
#include <QThread>
#include <QTimer>
#include <QTranslator>
#include <Qt>
#include <QtTypes>
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iterator>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <vector>

AppController::AppController(QObject* parent) : QObject(parent) {}

void AppController::loadSettings() {
    std::filesystem::path const configPath =
        std::filesystem::path(CorePaths::configFile().toStdString());

    m_settings = std::make_unique<AppSettings>();

    if (!std::filesystem::exists(configPath)) {
        Q_EMIT settingsLoaded(m_settings->toUiSettings());
        return;
    }

    if (!m_settings->load(configPath)) {
        if (!m_settings->save(configPath)) {
            Q_EMIT settingsLoaded(m_settings->toUiSettings());
            DialogUtils::showError(m_mainWindow, tr("Error"), tr("Can not save config file"));
        }
    }

    Q_EMIT initialSettingsLoaded(m_settings->toUiSettings());
}

void AppController::saveSettings() {
    std::filesystem::path const configPath =
        std::filesystem::path(CorePaths::configFile().toStdString());
    if (m_settings) {
        if (!m_settings->save(configPath)) {
            DialogUtils::showError(m_mainWindow, tr("Error"), tr("Can not save config file"));
        }
    }
}

void AppController::updateSettings(AppSettings const& newSettings) {
    if (!m_settings) {
        m_settings = std::make_unique<AppSettings>();
    } else {
        *m_settings = newSettings;
    }

    saveSettings();
}

void AppController::applyLanguage(UiConst::Language lang) {
    if (m_translator) { qApp->removeTranslator(m_translator.get()); }

    if (lang == UiConst::Language::Vietnamese) {
        m_translator = std::make_unique<QTranslator>();
        if (m_translator->load(":/i18n/app_vi.qm")) {
            qApp->installTranslator(m_translator.get());
        } else {
            m_translator.reset();
        }
    } else {
        m_translator.reset();
    }

    QEvent event(QEvent::LanguageChange);
    QCoreApplication::sendEvent(qApp, &event);

    // waitting for using
    // emit languageChanged();
}

void AppController::applyTheme(UiConst::Theme theme) {
    QString qssPath;
    QColor linkColor;

    switch (theme) {
        case UiConst::Theme::Light: {
            qssPath = ":/qss/light.qss";
            linkColor = QColor("#0000EE");
            break;
        }
        case UiConst::Theme::Dark: {
            qssPath = ":/qss/dark.qss";
            linkColor = QColor("#4FC3F7");
            break;
        }
    }

    QPalette palette = qApp->palette();
    palette.setColor(QPalette::Link, linkColor);
    qApp->setPalette(palette);

    QFile qssFile(qssPath);
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        QString const styleSheet = QString::fromUtf8(qssFile.readAll());
        qApp->setStyleSheet(styleSheet);
        qssFile.close();
    } else {
        // Nếu không mở được file theme, fallback sang mặc định
        qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setStyleSheet("");
    }

    // Thông báo font thay đổi để ResultsTable tự cascade
    qApp->setFont(qApp->font());

    // Áp dụng theme tô màu cho code editor
    if (m_mainWindow != nullptr) { m_mainWindow->applySyntaxHighlightingTheme(theme); }
}

void AppController::setMainWindow(MainWindow* window) {
    m_mainWindow = window;
}

void AppController::setCore(NotesAppCore* core) {
    m_core = core;
}

UpdateManager* AppController::updateManager() {
    if (!m_updateManager) {
        m_updateManager = std::make_unique<UpdateManager>(this);

        // Kết nối signal tới MainWindow
        if (m_mainWindow != nullptr) {
            QObject::connect(m_updateManager.get(), &UpdateManager::updateAvailable, m_mainWindow,
                             &MainWindow::onUpdateAvailable);
            QObject::connect(m_updateManager.get(), &UpdateManager::noUpdateAvailable, m_mainWindow,
                             &MainWindow::onNoUpdateAvailable);
            QObject::connect(m_updateManager.get(), &UpdateManager::updateCheckFailed, m_mainWindow,
                             &MainWindow::onUpdateCheckFailed);
        }
    }

    return m_updateManager.get();
}

DownloadManager* AppController::downloadManager() {
    if (!m_downloadManager) {
        m_downloadManager = std::make_unique<DownloadManager>(this);

        if (m_mainWindow != nullptr) {
            QObject::connect(m_downloadManager.get(), &DownloadManager::downloadStarted,
                             m_mainWindow, &MainWindow::onDownloadStarted);
            QObject::connect(m_downloadManager.get(), &DownloadManager::downloadProgress,
                             m_mainWindow, &MainWindow::onDownloadProgress);
            QObject::connect(m_downloadManager.get(), &DownloadManager::downloadFinished,
                             m_mainWindow, &MainWindow::onDownloadFinished);
            QObject::connect(m_downloadManager.get(), &DownloadManager::downloadFailed,
                             m_mainWindow, &MainWindow::onDownloadFailed);
            QObject::connect(m_downloadManager.get(),
                             &DownloadManager::downloadFailCauseTimeoutRequest, m_mainWindow,
                             &MainWindow::handleDownloadFailCauseTimeout);
        }
    }

    return m_downloadManager.get();
}

void AppController::oauthManager() {
    if (m_oauthManager == nullptr || m_GDService == nullptr) {
        m_oauthManager = std::make_unique<OAuthManager>();
        m_GDService = std::make_unique<GoogleDriveService>(m_oauthManager.get());

        if (m_oauthManager != nullptr) {
            QObject::connect(m_oauthManager.get(), &OAuthManager::gmailLinked, this,
                             &AppController::displayInfoGMUserLinked);
            QObject::connect(m_oauthManager.get(), &OAuthManager::gmailUnlinked, this,
                             &AppController::gmailUnlinked);

            QObject::connect(m_GDService.get(), &GoogleDriveService::onDownloadDBBtnRequest,
                             m_mainWindow, &MainWindow::startDownloadDBForward);
            QObject::connect(m_GDService.get(), &GoogleDriveService::onUploadDBBtnRequest,
                             m_mainWindow, &MainWindow::startUploadDBForward);

            QObject::connect(m_GDService.get(), &GoogleDriveService::returnDBInfo, m_mainWindow,
                             &MainWindow::returnDBInfoForward);

            QObject::connect(m_oauthManager.get(), &OAuthManager::loginFailed, m_mainWindow,
                             &MainWindow::loginFailedForward);

            QObject::connect(this, &AppController::cancelLoginRequestedForward,
                             m_oauthManager.get(), &OAuthManager::cancelCurrentLogin);

            QObject::connect(m_GDService.get(), &GoogleDriveService::closeConnectDBRequest, this,
                             &AppController::closeConnectDBRequestForward);
            QObject::connect(m_GDService.get(), &GoogleDriveService::reconnectDBRequest, this,
                             &AppController::reconnectDBRequestForward);

            QObject::connect(this, &AppController::dbClosedForward, m_GDService.get(),
                             &GoogleDriveService::onConnectClosedForUpload);
            QObject::connect(this, &AppController::dbClosedForward, m_GDService.get(),
                             &GoogleDriveService::onConnectClosedForDownload);

            QObject::connect(this, &AppController::deleteDatabaseFileRequest, m_GDService.get(),
                             &GoogleDriveService::handleDeleteDatabaseFileRequest);
            QObject::connect(m_GDService.get(), &GoogleDriveService::deleteDatabaseFileRespond,
                             this, &AppController::deleteDatabaseFileRespondForward);

            m_oauthManager->tryAutoLogin();
        }
    }
}

void AppController::handleGetAllDataRequest() {
    if (m_core == nullptr) { return; }

    auto allRes = m_core->getAllUnified();
    if (allRes.empty()) {
        if (m_mainWindow != nullptr) {
            m_mainWindow->updateStatus(tr("Database is empty"));
            return;
        }
    }

    Q_EMIT displayResultForGetAll(allRes);
}

void AppController::handleLoadResourceByTypeRequest(ResourceType type) {
    if (m_core == nullptr) { return; }

    auto res = m_core->getAllResourcesByType(type);
    if (res.empty()) {
        if (m_mainWindow != nullptr) {
            m_mainWindow->updateStatus(tr("Database is empty"));
            return;
        }
    }

    Q_EMIT displayResultForGetAll(res);
}

void AppController::handleDefaultSettingsRequest() {
    Q_EMIT settingsUpdateStatus(tr("Settings default!"), UiConst::SettingsMessageState::NotChange,
                                UiConst::SettingsTabNotiLevel::Caution);
}

void AppController::handleApplySettingsRequest(SettingsData const& data) {
    auto* settings = m_settings.get();
    if (settings == nullptr) {
        DialogUtils::showError(m_mainWindow, tr("Error"), tr("Core is not initialized."));
        return;
    }

    settings->setLanguage(data.language);
    settings->setTheme(data.theme);

    if (!data.resourceDir.empty()) { settings->setResourceDir(data.resourceDir); }

    settings->setManagedResources(data.isManagedResource);
    settings->setResourceDirCustomized(data.isResourceDirCustomized);
    settings->setCleanupEpubCache(data.isEpubCleanupCache);
    settings->setCleanupMDCache(data.isMDCleanupCache);
    settings->setExpiredCleanupEpubCache(data.expiredCleanupEpubCache);
    settings->setExpiredCleanupMDCache(data.expiredCleanupMDCache);

    if (settings->isDirty()) {
        applyLanguage(data.language);
        applyTheme(data.theme);

        if (!m_currentLinkedEmail.isEmpty()) {
            auto const htmlTextEmail =
                tr("Hello, ") +
                QString("<span style=\"color:%1;\"><i>%2</i></span>")
                    .arg((isDarkTheme()) ? "#FFB86C" : "#1A73E8", m_currentLinkedEmail);

            Q_EMIT gmailLinkedForView(htmlTextEmail);
        }

        saveSettings();
        settings->markDirty(false);

        Q_EMIT requestSyntaxHighlightingUpdate(data.theme);

        Q_EMIT settingsUpdateStatus(tr("Settings updated!"), UiConst::SettingsMessageState::Updated,
                                    UiConst::SettingsTabNotiLevel::Good);
    } else {
        Q_EMIT settingsUpdateStatus(tr("Nothing changed, settings not save"),
                                    UiConst::SettingsMessageState::None);
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void AppController::handleAddNoteRequest(QString const& title, QString const& textContent,
                                         QString const& filePath, QString const& url,
                                         QStringList const& tags, UiConst::AddResMode mode) {
    if (mode == UiConst::AddResMode::File && m_settings->isManagedResources()) {
        auto const& resDir = m_settings->resourceDir();
        bool const needStrictCheck = !m_settings->isDefaultResourceDir();

        if (needStrictCheck &&
            (!std::filesystem::exists(resDir) || !std::filesystem::is_directory(resDir))) {
            Q_EMIT addTabNotiRequest(tr("Resource directory does not exist. "
                                        "Please fix it in Settings before adding resources."),
                                     UiConst::SettingsTabNotiLevel::Warning);

            return;
        }
    }

    std::filesystem::path const filePathFs{filePath.toStdWString()};
    ResourceType type{};
    if (mode == UiConst::AddResMode::Text) {
        if (textContent.isEmpty()) {
            Q_EMIT addTabNotiRequest(tr("Content cannot be empty!"),
                                     UiConst::SettingsTabNotiLevel::Warning);
            return;
        }

        type = ResourceType::PlainText;
    } else if (mode == UiConst::AddResMode::File) {
        if (filePath.isEmpty()) {
            Q_EMIT addTabNotiRequest(tr("File path is empty."),
                                     UiConst::SettingsTabNotiLevel::Warning);
            return;
        }

        if (m_core->isFileIndexed(filePathFs)) {
            Q_EMIT addTabNotiRequest(tr("File exists in storage! Not add more."),
                                     UiConst::SettingsTabNotiLevel::Warning);
            return;
        }

        auto const typeOpt = resourceTypeFromFile(filePathFs);
        if (!typeOpt.has_value()) {
            Q_EMIT addTabNotiRequest(tr("File extension not support!"),
                                     UiConst::SettingsTabNotiLevel::Warning);
            return;
        }

        type = *typeOpt;
    } else if (mode == UiConst::AddResMode::Url) {
        if (url.isEmpty()) {
            Q_EMIT addTabNotiRequest(tr("Url cannot be empty!"),
                                     UiConst::SettingsTabNotiLevel::Warning);
            return;
        }

        type = ResourceType::Url;
    }

    auto const titleStd = title.toStdString();
    if (m_core->isExistTitle(titleStd, type)) {
        Q_EMIT addTabNotiRequest(tr("Title exists! Please choose another title"),
                                 UiConst::SettingsTabNotiLevel::Warning);
        return;
    }

    sqlite_int64 resId{};
    switch (mode) {
        case UiConst::AddResMode::Text: {
            resId = handleTextMode(titleStd, textContent, type);
            break;
        }
        case UiConst::AddResMode::File: {
            resId = handleFileMode(titleStd, filePathFs, type);
            break;
        }
        case UiConst::AddResMode::Url: {
            resId = handleUrlMode(titleStd, url, type);
            break;
        }
    }

    if (resId == 0) { return; }

    addTagsToResource(resId, tags);

    Q_EMIT resetAddTabInputsRequest();
}

void AppController::addTagsToResource(sqlite3_int64 resourceId, QStringList const& tags) const {
    if (tags.isEmpty()) { return; }

    std::vector<std::string> tagNames;
    tagNames.reserve(static_cast<std::size_t>(tags.size()));
    std::ranges::transform(tags, std::back_inserter(tagNames),
                           [](QString const& s) { return s.toStdString(); });
    m_core->addTags(resourceId, tagNames);
}

void AppController::handleSearchRequest(QString const& keyword, QString const& mode) {
    if (m_core == nullptr) {
        DialogUtils::showError(m_mainWindow, tr("Error"), tr("Database not initialized."));
        return;
    }

    if (keyword.isEmpty()) {
        DialogUtils::showInfo(m_mainWindow, tr("Information"),
                              tr("Please enter a keyword to search."));
        return;
    }

    auto* worker = new ResourceSearchWorker(m_core);
    worker->setSearchParams(keyword, mode);
    auto* thread = new QThread();
    worker->moveToThread(thread);

    QObject::connect(thread, &QThread::started, worker, &ResourceSearchWorker::doSearch);
    QObject::connect(
        worker, &ResourceSearchWorker::searchFinished, this,
        [this, mode](std::vector<UnifiedSearchResult> const& results) {
            Q_EMIT searchFinishedFromController(results, mode);
        },
        Qt::QueuedConnection);

    QObject::connect(worker, &ResourceSearchWorker::searchFinished, thread, &QThread::quit);
    QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

SettingsData AppController::currentUiSettings() const {
    return m_settings->toUiSettings();
}

SettingsData AppController::defaultUiSettings() {
    return AppSettings::defaultUiSettings();
}

void AppController::handleCheckUpdateRequested() {
    using Qt::Literals::StringLiterals::operator""_s;
    auto const kUpdateUrl = u"https://api.github.com/repos/yutijang/Notesman/releases/latest"_s;
    updateManager()->checkForUpdates(kUpdateUrl);
}

void AppController::onUpdateDecision(bool accepted, UpdateInfoSummary const& updateInfo) {
    if (!accepted) { return; }

    QUrl const kdownloadUrl(updateInfo.assetDownloadURL);
    QString const koutputPath = QDir::temp().filePath(updateInfo.assetName);

    m_lastUpdateInfoSummary = updateInfo;
    downloadManager()->startDownload(kdownloadUrl, koutputPath);
}

void AppController::handleLoginGMRequested() {
    if (m_oauthManager != nullptr) { m_oauthManager->handleLoginGMRequested(); }
}

void AppController::handleUnlinkGMRequested(bool isDeleteDB) {
    if (m_oauthManager == nullptr) { return; }

    if (isDeleteDB && m_GDService != nullptr) {
        QObject::connect(
            m_GDService.get(), &GoogleDriveService::deleteDatabaseFileRespond, this,
            [this](QString const& msg) {
                Log::info("Cleanup on Cloud finished: {}. Now revoking token...",
                          msg.toStdString());

                finalizeUnlink();
            },
            Qt::SingleShotConnection);

        Q_EMIT deleteDatabaseFileRequest();
    } else {
        finalizeUnlink();
    }
}

void AppController::finalizeUnlink() {
    m_currentLinkedEmail.clear();
    m_oauthManager->handleUnlinkGMRequested();
}

void AppController::uploadDbAuto() {
    if (m_GDService != nullptr) { m_GDService->uploadDbAuto(); }
}

void AppController::downloadDbAuto() {
    if (m_GDService != nullptr) { m_GDService->downloadDbAuto(); }
}

void AppController::updateTranslatedStrings() {
    if (!m_currentLinkedEmail.isEmpty()) {
        auto const htmlTextEmail =
            tr("Hello, ") + QString("<span style=\"color:%1;\"><i>%2</i></span>")
                                .arg((isDarkTheme()) ? "#FFB86C" : "#1A73E8", m_currentLinkedEmail);

        Q_EMIT gmailLinkedForView(htmlTextEmail);
    }
}

void AppController::handleGetDBInfoRequested() {
    if (m_GDService != nullptr) { m_GDService->getDBInfo(); }
}

void AppController::displayInfoGMUserLinked(QString const& email) {
    m_currentLinkedEmail = email;

    auto const htmlTextEmail =
        tr("Hello, ") + QString("<span style=\"color:%1;\"><i>%2</i></span>")
                            .arg((isDarkTheme()) ? "#FFB86C" : "#1A73E8", m_currentLinkedEmail);

    Q_EMIT gmailLinkedForView(htmlTextEmail);
}

sqlite_int64 AppController::handleTextMode(std::string const& title, QString const& textContent,
                                           ResourceType& outType) {
    auto resId = m_core->addTextNote(title, textContent.toUtf8().toStdString(), outType);
    Q_EMIT addTabNotiRequest(tr("Note added successfully!"), UiConst::SettingsTabNotiLevel::Good);
    return resId;
}

sqlite_int64 AppController::handleFileMode(std::string const& title,
                                           std::filesystem::path const& filePath,
                                           ResourceType& outType) {
    std::string contentToIndex;

    if (Utils::isIndexable(outType, filePath) == IndexableResult::Yes) {
        contentToIndex = Utils::readFileToString(filePath);
    }

    auto resId = m_core->addFileNote(filePath, title, outType, m_settings->isManagedResources(),
                                     contentToIndex);

    Q_EMIT addTabNotiRequest(tr("File added successfully!"), UiConst::SettingsTabNotiLevel::Good);

    return resId;
}

sqlite_int64 AppController::handleUrlMode(std::string const& title, QString const& url,
                                          ResourceType& outType) {
    sqlite3_int64 resId{};

    auto resIdOtp = m_core->addUrlNote(title, outType, url.toStdString());
    if (!resIdOtp) {
        Q_EMIT addTabNotiRequest(tr("Url added fail!"), UiConst::SettingsTabNotiLevel::Warning);
    } else {
        resId = *resIdOtp;
        Q_EMIT addTabNotiRequest(tr("Url added successfully!"),
                                 UiConst::SettingsTabNotiLevel::Good);
    }

    return resId;
}

UiConst::CleanupResult AppController::cleanupOldEpubCache(int days) {
    QDir const dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/epub");
    if (!dir.exists()) { return UiConst::CleanupResult::PathError; }

    if (dir.isEmpty()) { return UiConst::CleanupResult::AlreadyEmpty; }

    auto const entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    QDateTime const now = QDateTime::currentDateTime();

    if (days <= 0) {
        for (auto const& fi : entries) { QDir(fi.absoluteFilePath()).removeRecursively(); }
        return UiConst::CleanupResult::Success;
    }

    qint64 const maxAgeSecs = static_cast<qint64>(days) * 86400;

    for (auto const& fi : entries) {
        if (fi.lastModified().secsTo(now) >= maxAgeSecs) {
            QDir(fi.absoluteFilePath()).removeRecursively();
        }
    }

    return UiConst::CleanupResult::Success;
}

UiConst::CleanupResult AppController::cleanupOldMarkdownCache(int days) {
    QDir const dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/markdown");
    if (!dir.exists()) { return UiConst::CleanupResult::PathError; }

    if (dir.isEmpty()) { return UiConst::CleanupResult::AlreadyEmpty; }

    auto const entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    QDateTime const now = QDateTime::currentDateTime();

    if (days <= 0) {
        for (auto const& fi : entries) { QFile::remove(fi.absoluteFilePath()); }
        return UiConst::CleanupResult::Success;
    }

    qint64 const maxAgeSecs = static_cast<qint64>(days) * 86400;

    for (auto const& fi : entries) {
        if (fi.lastModified().secsTo(now) >= maxAgeSecs) { QFile::remove(fi.absoluteFilePath()); }
    }

    return UiConst::CleanupResult::Success;
}

UiConst::CleanupResult AppController::cleanupOldEpubCacheNow() {
    return cleanupOldEpubCache(0);
}

UiConst::CleanupResult AppController::cleanupOldMarkdownCacheNow() {
    return cleanupOldMarkdownCache(0);
}

void AppController::handleFileAssociationBtnRequest() {
#ifdef Q_OS_WIN
    bool ok = FileAssociation::isUpToDate();
    if (ok) {
        FileAssociation::unregisterAssociation();

        Q_EMIT settingsUpdateStatus("File association unregistered",
                                    UiConst::SettingsMessageState::Updated);
        ok = false;
    } else {
        if (FileAssociation::registerAssociation()) {
            // cập nhật status label: "Registered"
            Q_EMIT settingsUpdateStatus("File association registration successful",
                                        UiConst::SettingsMessageState::Updated);
            ok = true;
        } else {
            // cập nhật status label: "Failed"
            Q_EMIT settingsUpdateStatus("File association registration failed",
                                        UiConst::SettingsMessageState::Updated);
        }
    }

    Q_EMIT refreshFileAssociationStatus(ok);
#endif
}
