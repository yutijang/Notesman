#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <sqlite3.h>
#include <QObject>
#include <QTranslator>
#include <QTcpServer>
#include <QStringList>

#include "AppSettings.hpp"
#include "DownloadManager.hpp"
#include "GoogleDriveService.hpp"
#include "OAuthManager.hpp"
#include "UpdateManager.hpp"
#include "model.hpp"
#include "UiConstants.hpp"
#include "SettingsData.hpp"
#include "UpdateInfoSummary.hpp"

class NotesAppCore;
class MainWindow;

class AppController final : public QObject {
        Q_OBJECT

    public:
        explicit AppController(QObject* parent = nullptr);
        ~AppController() override = default;

        void loadSettings();
        void saveSettings();
        void updateSettings(AppSettings const& newSettings);

        [[nodiscard]] AppSettings const* settings() const noexcept { return m_settings.get(); }

        [[nodiscard]] bool isDarkTheme() const noexcept {
            return m_settings->theme() == UiConst::Theme::Dark;
        }

        [[nodiscard]] UiConst::Theme currentTheme() const noexcept { return m_settings->theme(); }

        [[nodiscard]] QString lastUpdateInfoAssetHash() const noexcept {
            return m_lastUpdateInfoSummary.assetHash;
        }

        [[nodiscard]] SettingsData currentUiSettings() const;
        [[nodiscard]] static SettingsData defaultUiSettings();

        [[nodiscard]] std::filesystem::path resourceDir() const noexcept {
            return m_settings->resourceDir();
        }

        [[nodiscard]] bool isManagedResources() const noexcept {
            return m_settings->isManagedResources();
        }

        void applyLanguage(UiConst::Language lang);
        void applyTheme(UiConst::Theme theme);
        void setMainWindow(MainWindow* window);
        void setCore(NotesAppCore* core);

        UpdateManager* updateManager();
        DownloadManager* downloadManager();
        void oauthManager();
        void updateTranslatedStrings();

        void handleGetAllDataRequest();

        void handleLoadResourceByTypeRequest(ResourceType type);
        void handleDefaultSettingsRequest();
        void handleApplySettingsRequest(SettingsData const& data);
        void handleAddNoteRequest(QString const& title, QString const& textContent,
                                  QString const& filePath, QString const& url,
                                  QStringList const& tags, UiConst::AddResMode mode);
        void handleSearchRequest(QString const& keyword, QString const& mode);
        void handleCheckUpdateRequested();
        void onUpdateDecision(bool accepted, UpdateInfoSummary const& updateInfo);
        void handleLoginGMRequested();
        void handleUnlinkGMRequested(bool isDeleteDB);
        void uploadDbAuto();
        void downloadDbAuto();

        void handleGetDBInfoRequested();

        static UiConst::CleanupResult cleanupOldEpubCache(int days);
        static UiConst::CleanupResult cleanupOldMarkdownCache(int days);
        static UiConst::CleanupResult cleanupOldEpubCacheNow();
        static UiConst::CleanupResult cleanupOldMarkdownCacheNow();

    Q_SIGNALS:
        // waitting for using
        // void languageChanged();

        void settingsLoaded(SettingsData const& settings);
        void displayResultForGetAll(std::vector<UnifiedSearchResult> const& results);
        void settingsUpdateStatus(
            QString const& message, UiConst::SettingsMessageState state,
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::Normal);
        void initialSettingsLoaded(SettingsData const& settings);
        void requestSyntaxHighlightingUpdate(UiConst::Theme theme);
        void addTabNotiRequest(QString const& message, UiConst::SettingsTabNotiLevel notiType =
                                                           UiConst::SettingsTabNotiLevel::Normal);
        void resetAddTabInputsRequest();
        void searchFinishedFromController(std::vector<UnifiedSearchResult> const& results,
                                          QString const& mode);
        void gmailUnlinked();
        void gmailLinkedForView(QString const& htmlTextEmail);
        void cancelLoginRequestedForward();
        void closeConnectDBRequestForward(bool isUpload);
        void reconnectDBRequestForward();
        void dbClosedForward(bool isUpload);
        void dbOpenedForward();
        void deleteDatabaseFileRequest();

        void deleteDatabaseFileRespondForward(QString const& msg);

    private: // NOLINT(readability-redundant-access-specifiers)
        void addTagsToResource(sqlite3_int64 resourceId, QStringList const& tags) const;
        void finalizeUnlink();
        sqlite_int64 handleTextMode(std::string const& title, QString const& textContent,
                                    ResourceType& outType);
        sqlite_int64 handleFileMode(std::string const& title, std::filesystem::path const& filePath,
                                    ResourceType& outType);
        sqlite_int64 handleUrlMode(std::string const& title, QString const& url,
                                   ResourceType& outType);

        void displayInfoGMUserLinked(QString const& email);

        std::unique_ptr<AppSettings> m_settings;
        std::unique_ptr<QTranslator> m_translator;
        std::unique_ptr<UpdateManager> m_updateManager;
        std::unique_ptr<DownloadManager> m_downloadManager;
        std::unique_ptr<OAuthManager> m_oauthManager;
        std::unique_ptr<GoogleDriveService> m_GDService;

        NotesAppCore* m_core{};
        MainWindow* m_mainWindow{};
        UpdateInfoSummary m_lastUpdateInfoSummary{};
        QString m_currentLinkedEmail;
};
