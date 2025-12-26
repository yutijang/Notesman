#pragma once

#include <filesystem>
#include <memory>
#include <vector>
#include <sqlite3.h>
#include <qcontainerfwd.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qtranslator.h>

#include "AppSettings.hpp"
#include "DownloadManager.hpp"
#include "GoogleDriveService.hpp"
#include "Language.hpp"
#include "OAuthManager.hpp"
#include "Theme.hpp"
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
        void updateSettings(const AppSettings &newSettings);

        [[nodiscard]] const AppSettings* settings() const noexcept { return m_settings.get(); }

        [[nodiscard]] bool isDarkTheme() const noexcept {
            return m_settings->theme() == Theme::dark;
        }

        [[nodiscard]] Theme currentTheme() const noexcept { return m_settings->theme(); }

        [[nodiscard]] QString lastUpdateInfoAssetHash() const noexcept {
            return m_lastUpdateInfoSummary.assetHash;
        }

        [[nodiscard]] SettingsData currentUiSettings() const;
        [[nodiscard]] static SettingsData defaultUiSettings();

        [[nodiscard]] std::filesystem::path resourceDir() const noexcept {
            return m_settings->resourceDir();
        }

        void applyLanguage(Language lang);
        void applyTheme(Theme theme);
        void setMainWindow(MainWindow* window);
        void setCore(NotesAppCore* core);

        UpdateManager* updateManager();
        DownloadManager* downloadManager();
        void oauthManager();
        void updateTranslatedStrings();

    public slots: // NOLINT(readability-redundant-access-specifiers)
        void handleGetAllDataRequest();
        void handleDefaultSettingsRequest();
        void handleApplySettingsRequest(const SettingsData &data);
        void handleAddNoteRequest(const QString &title, const QString &textContent,
                                  const QString &filePath, const QStringList &tags,
                                  bool isTextMode);
        void handleSearchRequest(const QString &keyword, const QString &mode);
        void handleCheckUpdateRequested();
        void onUpdateDecision(bool accepted, const UpdateInfoSummary &updateInfo);
        void handleLoginGMRequested();
        void handleUnlinkGMRequested(bool isDeleteDB);
        void uploadDbAuto();
        void downloadDbAuto();

        void handleGetDBInfoRequested();

    signals:
        // waitting for using
        // void languageChanged();

        void settingsLoaded(const SettingsData &settings);
        void displayResultForGetAll(const std::vector<FullResource> &results);
        void settingsUpdateStatus(
            const QString &message, UiConst::SettingsMessageState state,
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::normal);
        void initialSettingsLoaded(const SettingsData &settings);
        void requestSyntaxHighlightingUpdate(Theme theme);
        void addTabNotiRequest(const QString &message, UiConst::SettingsTabNotiLevel notiType =
                                                           UiConst::SettingsTabNotiLevel::normal);
        void resetAddTabInputsRequest();
        void searchFinishedFromController(const std::vector<FullResource> &results);
        void gmailUnlinked();
        void gmailLinkedForView(const QString &htmlTextEmail);
        void cancelLoginRequestedForward();
        void closeConnectDBRequestForward(bool isUpload);
        void reconnectDBRequestForward();
        void dbClosedForward(bool isUpload);
        void dbOpenedForward();
        void deleteDatabaseFileRequest();

        void deleteDatabaseFileRespondForward(const QString &msg);

    private:
        void addTagsToResource(sqlite3_int64 resourceId, const QStringList &tags) const;
        void finalizeUnlink();

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
