#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <QMainWindow>
#include <QModelIndexList>
#include <QtTypes>
#include <QString>
#include <QObject>
#include <QStringList>

#include "IResourceViewer.hpp"
#include "UiConstants.hpp"
#include "SettingsData.hpp"
#include "UpdateInfoSummary.hpp"
#include "model.hpp"
#include "ResourceViewService.hpp"

// ----------------------------------------------------
// Forward Declarations cho các Widgets con (Best Practice)
// ----------------------------------------------------
class QTabWidget;
class QLineEdit;
class CppHighlighter;
class NotesAppCore;
class QShowEvent;
class QCloseEvent;
class QWidget;
class TagInput;
class BrowseTabWidget;
class AddTabWidget;
class SettingsTabWidget;
class CodeEditorLineHighlighter;
class AppController;
class QProgressDialog;
class ResultsTable;
class InfoCornerWidget;
class QAction;

// ----------------------------------------------------

class MainWindow : public QMainWindow {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);
        ~MainWindow() override = default;

        void setAppController(AppController* controller);
        void retranslateUi();
        void applySyntaxHighlightingTheme(UiConst::Theme theme);
        void onUpdateAvailable(UpdateInfoSummary const& infoSummary);
        void onNoUpdateAvailable();
        void onUpdateCheckFailed(QString const& error);

        void setCore(NotesAppCore* core);

        void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
        void onDownloadStarted();
        void onDownloadFinished(QString const& filePath);
        void onDownloadFailed(QString const& errorString);
        void handleDownloadFailCauseTimeout();

        void updateStatus(QString const& message, int timeout = UiConst::NOTI_TIMEOUT5);

        void handleSyntaxHighlightingUpdate(UiConst::Theme theme);
        void handleSyntaxHighlightingFromAddTabRequested(bool checked);

    Q_SIGNALS:
        void requestDatabaseCreation();
        void requestUpdateCheck();
        void settingsTabShowNotification(
            QString const& message,
            UiConst::SettingsMessageState state = UiConst::SettingsMessageState::None,
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::Normal);
        void settingsStateChangeRequest(SettingsData const& settings);
        void checkUpdateRequest();
        void updateDecision(bool accepted, UpdateInfoSummary const& infoSummary);
        void settingsUiRefreshRequest(SettingsData const& settings);
        void updateColumnWidthsRequest();
        void startDownloadDBForward(
            bool isDisable, QString const& message = QString{},
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::Normal);
        void startUploadDBForward(
            bool isDisable, QString const& message = QString{},
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::Normal);
        void loginFailedForward(QString const& error = QString{});

        void returnDBInfoForward(QStringList const& res);

        void deleteDatabaseFileRespondForward(QString const& msg);
        void onCleanupFinished(UiConst::CleanupMode mode);

    protected:
        void showEvent(QShowEvent* event) override;
        void closeEvent(QCloseEvent* event) override;
        void changeEvent(QEvent* event) override;

    private: // NOLINT(readability-redundant-access-specifiers)
        // Build UI internal
        void buildUi();
        void setupBrowseTab();
        void setupAddTab();
        void setupSettingsTab();
        void setupIconInfo();
        void viewResource(std::int64_t id, ResourceType type, QString const& title,
                          QString const& path, QString const& url);
        void showContextMenu(QPoint const& pos, std::int64_t id, ResourceType type,
                             QString const& title, QString const& path, QString const& url);
        static void removeSelectedRowsFromTable(ResultsTable* table,
                                                QModelIndexList const& selectedRows);
        static sqlite3_int64 extractIdFromRow(ResultsTable* resultTable, int row);
        static std::optional<ResourceType> extractTypeFromRow(ResultsTable* resultTable, int row);
        void runUpdate(QString const& filePath);
        void disableSyntaxHighlightingTheme();
        QString resolveResPath(QString const& path);

        void onCheckUpdateClicked();
        void onAbout();

        void handleSettingsStateChange(UiConst::SettingsMessageState state);
        void handleContextMenuDeleteAction(ResultsTable* resultTable);

        void notiFromCleanupCacheResult(UiConst::CleanupResult result, UiConst::CleanupMode mode);

#if defined(Q_OS_WIN)
        void handleWindowsUpdate(const QString& filePath);
#elif defined(Q_OS_LINUX)
        void handleLinuxUpdate(const QString& filePath);

        std::unique_ptr<IResourceViewer> m_externalViewer;
        bool m_viewerLocked{};
#endif

        NotesAppCore* m_core{};
        AppController* m_appController{};

        // Widgets
        QTabWidget* m_tabWidget{};
        BrowseTabWidget* m_browseTab{};
        AddTabWidget* m_addTab{};
        SettingsTabWidget* m_settingsTab{};

        // Browse Tab
        QAction* m_deleteResourceAction{};
        ResultsTable* m_resultsTbl{};

        // Add Tab
        CppHighlighter* m_cppHighlighter{};
        CodeEditorLineHighlighter* m_lineHighlighter{};

        // Settings Tab

        InfoCornerWidget* m_infoWidget{};

        UiConst::SettingsMessageState m_settingsMessageState{UiConst::SettingsMessageState::None};

        QProgressDialog* m_progressDialog{};

        // ResourceViewService* m_resourceViewService{};
        std::unique_ptr<ResourceViewService> m_resourceViewService;
};
