#pragma once

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
        void onUpdateAvailable(const UpdateInfoSummary &infoSummary);
        void onNoUpdateAvailable();
        void onUpdateCheckFailed(const QString &error);

        void setCore(NotesAppCore* core);

        void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
        void onDownloadStarted();
        void onDownloadFinished(const QString &filePath);
        void onDownloadFailed(const QString &errorString);
        void handleDownloadFailCauseTimeout();

        void updateStatus(const QString &message, int timeout = UiConst::NOTI_TIMEOUT5);

        void handleSyntaxHighlightingUpdate(UiConst::Theme theme);
        void handleSyntaxHighlightingFromAddTabRequested(bool checked);

    Q_SIGNALS:
        void requestDatabaseCreation();
        void requestUpdateCheck();
        void settingsTabShowNotification(
            const QString &message,
            UiConst::SettingsMessageState state = UiConst::SettingsMessageState::none,
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::normal);
        void settingsStateChangeRequest(const SettingsData &settings);
        void checkUpdateRequest();
        void updateDecision(bool accepted, const UpdateInfoSummary &infoSummary);
        void settingsUiRefreshRequest(const SettingsData &settings);
        void updateColumnWidthsRequest();
        void startDownloadDBForward(
            bool isDisable, const QString &message = QString{},
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::normal);
        void startUploadDBForward(
            bool isDisable, const QString &message = QString{},
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::normal);
        void loginFailedForward(const QString &error = QString{});

        void returnDBInfoForward(const QStringList &res);

        void deleteDatabaseFileRespondForward(const QString &msg);

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
        void viewResource(int id, ResourceType type, const QString &title, const QString &path,
                          const QString &url);
        void showContextMenu(const QPoint &pos, int id, ResourceType type, const QString &title,
                             const QString &path, const QString &url);
        static void removeSelectedRowsFromTable(ResultsTable* table,
                                                const QModelIndexList &selectedRows);
        static sqlite3_int64 extractIdFromRow(ResultsTable* resultTable, int row);
        static std::optional<ResourceType> extractTypeFromRow(ResultsTable* resultTable, int row);
        void runUpdate(const QString &filePath);
        void disableSyntaxHighlightingTheme();
        QString resolveResPath(const QString &path);

        void onCheckUpdateClicked();
        void onAbout();

        void handleSettingsStateChange(UiConst::SettingsMessageState state);
        void handleContextMenuDeleteAction(ResultsTable* resultTable);

#if defined(Q_OS_WIN)
        void handleWindowsUpdate(const QString &filePath);
#elif defined(Q_OS_LINUX)
        void handleLinuxUpdate(const QString &filePath);

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

        UiConst::SettingsMessageState m_settingsMessageState{UiConst::SettingsMessageState::none};

        QProgressDialog* m_progressDialog{};

        // ResourceViewService* m_resourceViewService{};
        std::unique_ptr<ResourceViewService> m_resourceViewService;
};
