#include "application/AppController.hpp"

#include "application/FileAssociation.hpp"
#include "application/NotesAppCore.hpp"
#include "application/services/DownloadManager.hpp"
#include "application/services/GoogleDriveService.hpp"
#include "application/services/OAuthManager.hpp"
#include "application/services/UpdateInfoSummary.hpp"
#include "application/services/UpdateManager.hpp"
#include "application/workers/ResourceSearchWorker.hpp"
#include "common/logger/Logger.hpp"
#include "common/viewer_pack/SanitizeFileName.hpp"
#include "common/viewer_pack/ViewerPackHeader.hpp"
#include "common/viewer_pack/ViewerPackWriter.hpp"
#include "core/model/model.hpp"
#include "gui/MainWindow.hpp"
#include "gui/Settings/AppSettings.hpp"
#include "gui/Settings/SettingsData.hpp"
#include "gui/UiConstants.hpp"
#include "helper/CorePaths.hpp"
#include "helper/DialogUtils.hpp"
#include "helper/SettingsManager.hpp"
#include "helper/helper.hpp"

#include <QApplication>
#include <QDir>
#include <QFileDialog>
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
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <memory>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>

AppController::AppController(QObject* parent) : QObject(parent) {}

AppController::~AppController() = default;

void AppController::loadSettings() {
    std::filesystem::path const configPath =
        std::filesystem::path(CorePaths::configFile().toStdString());

    if (!std::filesystem::exists(configPath)) {
        Q_EMIT settingsLoaded(m_settings.toUiSettings());
        return;
    }

    if (!m_settings.load(configPath)) {
        if (!m_settings.save(configPath)) {
            Q_EMIT settingsLoaded(m_settings.toUiSettings());
            DialogUtils::showError(m_mainWindow, tr("Error"), tr("Can not save config file"));
        }
    }

    Q_EMIT initialSettingsLoaded(m_settings.toUiSettings());
}

void AppController::saveSettings() {
    std::filesystem::path const configPath =
        std::filesystem::path(CorePaths::configFile().toStdString());

    if (!m_settings.save(configPath)) {
        DialogUtils::showError(m_mainWindow, tr("Error"), tr("Can not save config file"));
    }
}

void AppController::updateSettings(AppSettings const& newSettings) {
    m_settings = newSettings;
    saveSettings();
}

void AppController::applyLanguage(UiConst::Language lang) {
    if (m_translator) {
        qApp->removeTranslator(m_translator.get());
    }

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
    if (m_mainWindow != nullptr) {
        m_mainWindow->applySyntaxHighlightingTheme(theme);
    }
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
            QObject::connect(m_updateManager.get(),
                             &UpdateManager::updateAvailable,
                             m_mainWindow,
                             &MainWindow::onUpdateAvailable);
            QObject::connect(m_updateManager.get(),
                             &UpdateManager::noUpdateAvailable,
                             m_mainWindow,
                             &MainWindow::onNoUpdateAvailable);
            QObject::connect(m_updateManager.get(),
                             &UpdateManager::updateCheckFailed,
                             m_mainWindow,
                             &MainWindow::onUpdateCheckFailed);
        }
    }

    return m_updateManager.get();
}

DownloadManager* AppController::downloadManager() {
    if (!m_downloadManager) {
        m_downloadManager = std::make_unique<DownloadManager>(this);

        if (m_mainWindow != nullptr) {
            QObject::connect(m_downloadManager.get(),
                             &DownloadManager::downloadStarted,
                             m_mainWindow,
                             &MainWindow::onDownloadStarted);
            QObject::connect(m_downloadManager.get(),
                             &DownloadManager::downloadProgress,
                             m_mainWindow,
                             &MainWindow::onDownloadProgress);
            QObject::connect(m_downloadManager.get(),
                             &DownloadManager::downloadFinished,
                             m_mainWindow,
                             &MainWindow::onDownloadFinished);
            QObject::connect(m_downloadManager.get(),
                             &DownloadManager::downloadFailed,
                             m_mainWindow,
                             &MainWindow::onDownloadFailed);
            QObject::connect(m_downloadManager.get(),
                             &DownloadManager::downloadTimedOut,
                             m_mainWindow,
                             &MainWindow::handleDownloadFailCauseTimeout);
        }
    }

    return m_downloadManager.get();
}

void AppController::ensureOAuth() {
    if (m_drive != nullptr) {
        return;
    }

    m_drive = std::make_unique<GDContext>(this);

    auto* oauth = m_drive->oauth.get();
    auto* service = m_drive->service.get();

    QObject::connect(
        oauth, &OAuthManager::gmailLinked, this, &AppController::displayInfoGMUserLinked);
    QObject::connect(oauth, &OAuthManager::gmailUnlinked, this, &AppController::gmailUnlinked);

    QObject::connect(service,
                     &GoogleDriveService::onDownloadDBBtnRequest,
                     m_mainWindow,
                     &MainWindow::startDownloadDBForward);
    QObject::connect(service,
                     &GoogleDriveService::onUploadDBBtnRequest,
                     m_mainWindow,
                     &MainWindow::startUploadDBForward);

    QObject::connect(
        service, &GoogleDriveService::returnDBInfo, m_mainWindow, &MainWindow::returnDBInfoForward);

    QObject::connect(
        oauth, &OAuthManager::loginFailed, m_mainWindow, &MainWindow::loginFailedForward);

    QObject::connect(this,
                     &AppController::cancelLoginRequestedForward,
                     oauth,
                     &OAuthManager::cancelCurrentLogin);

    QObject::connect(service,
                     &GoogleDriveService::closeConnectDBRequest,
                     this,
                     &AppController::closeConnectDBRequestForward);
    QObject::connect(service,
                     &GoogleDriveService::reconnectDBRequest,
                     this,
                     &AppController::reconnectDBRequestForward);

    QObject::connect(this,
                     &AppController::dbClosedForward,
                     service,
                     &GoogleDriveService::onConnectClosedForUpload);
    QObject::connect(this,
                     &AppController::dbClosedForward,
                     service,
                     &GoogleDriveService::onConnectClosedForDownload);

    QObject::connect(this,
                     &AppController::deleteDatabaseFileRequest,
                     service,
                     &GoogleDriveService::handleDeleteDatabaseFileRequest);
    QObject::connect(service,
                     &GoogleDriveService::deleteDatabaseFileRespond,
                     this,
                     &AppController::deleteDatabaseFileRespondForward);

    oauth->tryAutoLogin();
}

void AppController::handleGetAllDataRequest() {
    if (m_core == nullptr) {
        return;
    }

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
    if (m_core == nullptr) {
        return;
    }

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
    Q_EMIT settingsUpdateStatus(tr("Settings default!"),
                                UiConst::SettingsMessageState::NotChange,
                                UiConst::SettingsTabNotiLevel::Caution);
}

void AppController::handleApplySettingsRequest(SettingsData const& data) {
    m_settings.setLanguage(data.language);
    m_settings.setTheme(data.theme);

    if (!data.resourceDir.empty()) {
        m_settings.setResourceDir(data.resourceDir);
    }

    m_settings.setManagedResources(data.isManagedResource);
    m_settings.setResourceDirCustomized(data.isResourceDirCustomized);
    m_settings.setCleanupEpubCache(data.isEpubCleanupCache);
    m_settings.setCleanupMDCache(data.isMDCleanupCache);
    m_settings.setExpiredCleanupEpubCache(data.expiredCleanupEpubCache);
    m_settings.setExpiredCleanupMDCache(data.expiredCleanupMDCache);

    if (m_settings.isDirty()) {
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
        m_settings.markDirty(false);

        Q_EMIT requestSyntaxHighlightingUpdate(data.theme);

        Q_EMIT settingsUpdateStatus(tr("Settings updated!"),
                                    UiConst::SettingsMessageState::Updated,
                                    UiConst::SettingsTabNotiLevel::Good);
    } else {
        Q_EMIT settingsUpdateStatus(tr("Nothing changed, settings not save"),
                                    UiConst::SettingsMessageState::None);
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void AppController::handleAddNoteRequest(QString const& title,
                                         QString const& textContent,
                                         QString const& filePath,
                                         QString const& url,
                                         QStringList const& tags,
                                         UiConst::AddResMode mode) {
    if (mode == UiConst::AddResMode::File && m_settings.isManagedResources()) {
        auto const& resDir = m_settings.resourceDir();
        bool const needStrictCheck = !m_settings.isDefaultResourceDir();

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

    if (resId == 0) {
        return;
    }

    addTagsToResource(resId, tags);

    Q_EMIT resetAddTabInputsRequest();
}

void AppController::addTagsToResource(sqlite3_int64 resourceId, QStringList const& tags) const {
    if (tags.isEmpty()) {
        return;
    }

    std::vector<std::string> tagNames;
    tagNames.reserve(static_cast<std::size_t>(tags.size()));
    std::ranges::transform(tags, std::back_inserter(tagNames), [](QString const& s) {
        return s.toStdString();
    });
    m_core->addTags(resourceId, tagNames);
}

void AppController::handleSearchRequest(QString const& keyword, QString const& mode) {
    if (m_core == nullptr) {
        DialogUtils::showError(m_mainWindow, tr("Error"), tr("Database not initialized."));
        return;
    }

    if (keyword.isEmpty()) {
        DialogUtils::showInfo(
            m_mainWindow, tr("Information"), tr("Please enter a keyword to search."));
        return;
    }

    auto* worker = new ResourceSearchWorker(m_core);
    worker->setSearchParams(keyword, mode);
    auto* thread = new QThread();
    worker->moveToThread(thread);

    QObject::connect(thread, &QThread::started, worker, &ResourceSearchWorker::doSearch);
    QObject::connect(
        worker,
        &ResourceSearchWorker::searchFinished,
        this,
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
    return m_settings.toUiSettings();
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
    if (!accepted) {
        return;
    }

    QUrl const kdownloadUrl(updateInfo.assetDownloadURL);
    QString const koutputPath = QDir::temp().filePath(updateInfo.assetName);

    m_lastUpdateInfoSummary = updateInfo;
    downloadManager()->startDownload(kdownloadUrl, koutputPath);
}

void AppController::handleLoginGMRequested() {
    if (m_drive != nullptr) {
        m_drive->oauth->handleLoginGMRequested();
    }
}

void AppController::handleUnlinkGMRequested(bool isDeleteDB) {
    if (m_drive == nullptr) {
        return;
    }

    auto* service = m_drive->service.get();

    if (isDeleteDB) {
        QObject::connect(
            service,
            &GoogleDriveService::deleteDatabaseFileRespond,
            this,
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
    m_drive->oauth->handleUnlinkGMRequested();
}

void AppController::uploadDbAuto() {
    if (m_drive != nullptr) {
        m_drive->service->uploadDbAuto();
    }
}

void AppController::downloadDbAuto() {
    if (m_drive != nullptr) {
        m_drive->service->downloadDbAuto();
    }
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
    if (m_drive != nullptr) {
        m_drive->service->getDBInfo();
    }
}

void AppController::displayInfoGMUserLinked(QString const& email) {
    m_currentLinkedEmail = email;

    auto const htmlTextEmail =
        tr("Hello, ") + QString("<span style=\"color:%1;\"><i>%2</i></span>")
                            .arg((isDarkTheme()) ? "#FFB86C" : "#1A73E8", m_currentLinkedEmail);

    Q_EMIT gmailLinkedForView(htmlTextEmail);
}

sqlite_int64 AppController::handleTextMode(std::string const& title,
                                           QString const& textContent,
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

    auto resId = m_core->addFileNote(
        filePath, title, outType, m_settings.isManagedResources(), contentToIndex);

    Q_EMIT addTabNotiRequest(tr("File added successfully!"), UiConst::SettingsTabNotiLevel::Good);

    return resId;
}

sqlite_int64 AppController::handleUrlMode(std::string const& title,
                                          QString const& url,
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
    if (!dir.exists()) {
        return UiConst::CleanupResult::PathError;
    }

    if (dir.isEmpty()) {
        return UiConst::CleanupResult::AlreadyEmpty;
    }

    auto const entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    QDateTime const now = QDateTime::currentDateTime();

    if (days <= 0) {
        for (auto const& fi : entries) {
            QDir(fi.absoluteFilePath()).removeRecursively();
        }
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
    if (!dir.exists()) {
        return UiConst::CleanupResult::PathError;
    }

    if (dir.isEmpty()) {
        return UiConst::CleanupResult::AlreadyEmpty;
    }

    auto const entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    QDateTime const now = QDateTime::currentDateTime();

    if (days <= 0) {
        for (auto const& fi : entries) {
            QFile::remove(fi.absoluteFilePath());
        }
        return UiConst::CleanupResult::Success;
    }

    qint64 const maxAgeSecs = static_cast<qint64>(days) * 86400;

    for (auto const& fi : entries) {
        if (fi.lastModified().secsTo(now) >= maxAgeSecs) {
            QFile::remove(fi.absoluteFilePath());
        }
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
}

void AppController::createPackerFile(std::int64_t id, QString const& title) {
    auto const theme = currentTheme();
    auto const language = currentLanguage();

    QString const suggestedName =
        QString::fromStdString(ViewerPackUltis::sanitizeFileName(title.toStdString())) + ".rvpk";

    auto& qSettings = SettingsManager::instance();
    QString const desktopDirAsDefault =
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString const kDefaultDir = qSettings.get("packer/lastSaveDir", desktopDirAsDefault).toString();
    QString const savePath = QFileDialog::getSaveFileName(m_mainWindow,
                                                          tr("Save Shortcut"),
                                                          QDir(kDefaultDir).filePath(suggestedName),
                                                          tr("Viewer Pack (*.rvpk)"));

    if (savePath.isEmpty()) {
        return;
    } // user cancel

    ViewerPackHeader hdr{};

    // NOLINTNEXTLINE (-Wunsafe-buffer-usage-in-libc-call)
    std::memcpy(hdr.magic, ViewerPackHeader::RVPK_MAGIC, sizeof(ViewerPackHeader::RVPK_MAGIC));
    hdr.version = ViewerPackHeader::VERSION;
    hdr.headerSize = sizeof(ViewerPackHeader);
    hdr.resourceId = id;

    auto const uuidOpt = m_core->getResourceUuid(id);
    if (!uuidOpt || uuidOpt->size() != ViewerPackHeader::UUID_LENGTH) {
        throw std::runtime_error("Invalid uuid");
    }
    // NOLINTNEXTLINE (-Wunsafe-buffer-usage-in-libc-call)
    std::memcpy(hdr.uuid, uuidOpt->data(), ViewerPackHeader::UUID_LENGTH);

    hdr.themeMode = static_cast<std::uint8_t>(theme);
    hdr.language = static_cast<std::uint8_t>(language);
    hdr.reserved1 = 0;

    ViewerPackWriter writer;
    auto result = writer.write(savePath.toStdString(), hdr);

    if (!result.has_value()) {
        Log::err("failed to write .rvpk for resource {}", id);
        DialogUtils::showError(m_mainWindow,
                               tr("Error"),
                               tr("Failed to create shortcut file.\nPlease check write permissions "
                                  "for the selected location."));
        return;
    }

    QFileInfo const packerFile(savePath);
    qSettings.set("packer/lastSaveDir", packerFile.absoluteDir().path());

    DialogUtils::showInfo(m_mainWindow,
                          tr("Shortcut Created"),
                          tr("Shortcut created successfully:\n%1").arg(savePath));
}
