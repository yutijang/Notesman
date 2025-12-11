#include <memory>
#include <filesystem>
#include <QObject>
#include <QFileInfo>
#include <QDir>
#include <QString>
#include <QTranslator>
#include <QApplication>
#include <QStyleFactory>
#include <QTimer>
#include <QThread>
#include <QRegularExpression>

#include "AppController.hpp"
#include "MainWindow.hpp"
#include "NotesAppCore.hpp"
#include "ResourceSearchWorker.hpp"
#include "UpdateInfoSummary.hpp"
#include "DialogUtils.hpp"
#include "CorePaths.hpp"

AppController::AppController(QObject* parent) : QObject(parent) {}

void AppController::loadSettings() {
    const std::filesystem::path configPath =
        std::filesystem::path(CorePaths::configFile().toStdString());

    m_settings = std::make_unique<AppSettings>();

    if (!std::filesystem::exists(configPath)) {
        emit settingsLoaded(m_settings->toUiSettings());
        return;
    }

    if (!m_settings->load(configPath)) {
        if (!m_settings->save(configPath)) {
            emit settingsLoaded(m_settings->toUiSettings());
            DialogUtils::showError(m_mainWindow, tr("Error"), tr("Can not save config file"));
        }
    }

    emit initialSettingsLoaded(m_settings->toUiSettings());
}

void AppController::saveSettings() {
    const std::filesystem::path configPath =
        std::filesystem::path(CorePaths::configFile().toStdString());
    if (m_settings) {
        if (!m_settings->save(configPath)) {
            DialogUtils::showError(m_mainWindow, tr("Error"), tr("Can not save config file"));
        }
    }
}

void AppController::updateSettings(const AppSettings &newSettings) {
    if (!m_settings) {
        m_settings = std::make_unique<AppSettings>();
    } else {
        *m_settings = newSettings;
    }

    saveSettings();
}

void AppController::applyLanguage(Language lang) {
    if (m_translator) { qApp->removeTranslator(m_translator.get()); }

    if (lang == Language::vietnamese) {
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

void AppController::applyTheme(Theme theme) {
    QString qssPath;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch-default"
    switch (theme) {
        case Theme::light: qssPath = ":/qss/light.qss"; break;
        case Theme::dark : qssPath = ":/qss/dark.qss"; break;
    }
#pragma clang diagnostic pop

    QFile qssFile(qssPath);
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        const QString styleSheet = QString::fromUtf8(qssFile.readAll());
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

            QObject::connect(this, &AppController::downloadCanceledForward, m_downloadManager.get(),
                             &DownloadManager::handleDownloadCanceledRequest);
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
                             [this](const QString &email) {
                                 m_currentLinkedEmail = email;

                                 const auto htmlTextEmail =
                                     tr("Hello, ") +
                                     QString("<span style=\"color:%1;\"><i>%2</i></span>")
                                         .arg((isDarkTheme()) ? "#FFB86C" : "#1A73E8",
                                              m_currentLinkedEmail);

                                 emit gmailLinkedForView(htmlTextEmail);
                             });
            QObject::connect(m_oauthManager.get(), &OAuthManager::gmailUnlinked, this,
                             &AppController::gmailUnlinked);

            QObject::connect(m_GDService.get(), &GoogleDriveService::onDownloadDBBtnRequest,
                             m_mainWindow, &MainWindow::startDownloadDBForward);
            QObject::connect(m_GDService.get(), &GoogleDriveService::onUploadDBBtnRequest,
                             m_mainWindow, &MainWindow::startUploadDBForward);

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

            m_oauthManager->tryAutoLogin();
        }
    }
}

void AppController::handleGetAllDataRequest() {
    if (m_core == nullptr) { return; }

    const auto &allRes = m_core->getAllFull();
    if (allRes.empty()) {
        if (m_mainWindow != nullptr) {
            m_mainWindow->updateStatus(tr("Database is empty"));
            return;
        }
    }

    emit displayResultForGetAll(allRes);
}

void AppController::handleDefaultSettingsRequest() {
    emit settingsUpdateStatus(tr("Settings default!"), UiConst::SettingsMessageState::Default);
}

void AppController::handleApplySettingsRequest(const SettingsData &data) {
    auto* settings = m_settings.get();
    if (settings == nullptr) {
        DialogUtils::showError(m_mainWindow, tr("Error"), tr("Core is not initialized."));
        return;
    }

    settings->setLanguage(data.language);
    settings->setTheme(data.theme);

    if (!data.resourceDir.empty()) { settings->setResourceDir(data.resourceDir); }

    settings->setManagedResources(data.isManagedResource);

    if (settings->isDirty()) {
        applyLanguage(data.language);
        applyTheme(data.theme);

        if (!m_currentLinkedEmail.isEmpty()) {
            const auto htmlTextEmail =
                tr("Hello, ") +
                QString("<span style=\"color:%1;\"><i>%2</i></span>")
                    .arg((isDarkTheme()) ? "#FFB86C" : "#1A73E8", m_currentLinkedEmail);

            emit gmailLinkedForView(htmlTextEmail);
        }

        saveSettings();
        settings->markDirty(false);

        emit requestSyntaxHighlightingUpdate(data.theme);

        emit settingsUpdateStatus(tr("Settings updated!"), UiConst::SettingsMessageState::Updated);
    } else {
        emit settingsUpdateStatus(tr("Nothing changed, settings not save"),
                                  UiConst::SettingsMessageState::None);
    }
}

void AppController::handleAddNoteRequest(const QString &title, const QString &textContent,
                                         const QString &filePath, const QStringList &tags,
                                         bool isTextMode) {
    ResourceType type{};
    if (isTextMode) {
        type = ResourceType::text;
    } else {
        if (filePath.isEmpty()) {
            emit addTabNotiRequest(tr("File path is empty."));
            return;
        }

        if (m_core->isFileIndexed(filePath.toStdString())) {
            emit addTabNotiRequest(tr("File exists in storage! Not add more."));
            return;
        }

        const auto typeOpt = resourceTypeFromFile(filePath.toStdString());
        if (!typeOpt.has_value()) {
            emit addTabNotiRequest(tr("File extension not support!"));
            return;
        }

        type = *typeOpt;
    }

    if (m_core->isExistTitle(title.toStdString(), type)) {
        emit addTabNotiRequest(tr("Title exists! Please choose another title"));
        return;
    }

    if (isTextMode) {
        if (textContent.isEmpty()) {
            emit addTabNotiRequest(tr("Content cannot be empty!"));
            return;
        }

        const auto resId =
            m_core->addTextNote(title.toStdString(), textContent.toUtf8().toStdString(), type);

        addTagsToResource(resId, tags);

        emit addTabNotiRequest(tr("Note added successfully!"));
    } else {
        const auto resId = m_core->addFileNote(filePath.toStdString(), title.toStdString(), type,
                                               m_settings->isManagedResources());

        addTagsToResource(resId, tags);

        emit addTabNotiRequest(tr("File added successfully!"));
    }

    emit resetAddTabInputsRequest();
}

void AppController::addTagsToResource(sqlite3_int64 resourceId, const QStringList &tags) const {
    if (tags.isEmpty()) { return; }

    std::vector<std::string> tagNames;
    tagNames.reserve(static_cast<std::size_t>(tags.size()));
    std::ranges::transform(tags, std::back_inserter(tagNames),
                           [](const QString &s) { return s.toStdString(); });
    m_core->addTags(resourceId, tagNames);
}

void AppController::handleSearchRequest(const QString &keyword, const QString &mode) {
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
        [this, thread](const std::vector<FullResource> &results) {
            emit searchFinishedFromController(results);
            thread->quit();
        },
        Qt::QueuedConnection);

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
    const auto kUpdateUrl = u"https://api.github.com/repos/yutijang/Notesman/releases/latest"_s;
    updateManager()->checkForUpdates(kUpdateUrl);
}

void AppController::onUpdateDecision(bool accepted, const UpdateInfoSummary &updateInfo) {
    if (!accepted) { return; }

    const QUrl kdownloadUrl(updateInfo.assetDownloadURL);
    const QString koutputPath = QDir::temp().filePath(updateInfo.assetName);

    m_lastUpdateInfoSummary = updateInfo;
    downloadManager()->startDownload(kdownloadUrl, koutputPath);
}

void AppController::handleLoginGMRequested() {
    if (m_oauthManager != nullptr) { m_oauthManager->handleLoginGMRequested(); }
}

void AppController::handleUnlinkGMRequested() {
    if (m_oauthManager != nullptr) {
        m_currentLinkedEmail.clear();
        m_oauthManager->handleUnlinkGMRequested();
    }
}

void AppController::uploadDbAuto() {
    if (m_GDService != nullptr) { m_GDService->uploadDbAuto(); }
}

void AppController::downloadDbAuto() {
    if (m_GDService != nullptr) { m_GDService->downloadDbAuto(); }
}

void AppController::updateTranslatedStrings() {
    if (!m_currentLinkedEmail.isEmpty()) {
        const auto htmlTextEmail =
            tr("Hello, ") + QString("<span style=\"color:%1;\"><i>%2</i></span>")
                                .arg((isDarkTheme()) ? "#FFB86C" : "#1A73E8", m_currentLinkedEmail);

        emit gmailLinkedForView(htmlTextEmail);
    }
}
