#pragma once

#include <QMainWindow>
#include <QModelIndexList>
#include <sqlite3.h>

#include "UiConstants.hpp"
#include "Theme.hpp"
#include "SettingsData.hpp"
#include "UpdateInfoSummary.hpp"
#include "PlainTextEdit.hpp"

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

// ----------------------------------------------------

class MainWindow : public QMainWindow {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);
        ~MainWindow() override = default;

        void setAppController(AppController* controller);
        void retranslateUi();
        void applySyntaxHighlightingTheme(Theme theme);
        void onUpdateAvailable(const UpdateInfoSummary &infoSummary);
        void onNoUpdateAvailable();
        void onUpdateCheckFailed(const QString &error);

    signals:
        void requestDatabaseInit(); // Gửi tín hiệu cho AppController/AppInitializer
        void requestDatabaseCreation();
        void requestUpdateCheck();
        void settingsTabShowNotification(const QString &message);
        void settingsStateChangeRequest(const SettingsData &settings);
        void checkUpdateRequest();
        void updateDecision(bool accepted, const UpdateInfoSummary &infoSummary);
        void settingsUiRefreshRequest(const SettingsData &settings);
        void updateColumnWidthsRequest();
        void startDownloadDBForward(bool isDisable, const QString &message = QString{});
        void startUploadDBForward(bool isDisable, const QString &message = QString{});
        void loginFailedForward(const QString &error = QString{});

    public slots:
        void setCore(NotesAppCore* core);
        void onCheckUpdateClicked();
        void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
        void onDownloadStarted();
        void onDownloadFinished(const QString &filePath);
        void onDownloadFailed(const QString &errorString);
        void updateStatus(const QString &message,
                          int timeout = 5000); // NOLINT(readability-magic-numbers)
        void handleDownloadFailCauseTimeout();

    protected:
        void showEvent(QShowEvent* event) override;
        void closeEvent(QCloseEvent* event) override;
        void changeEvent(QEvent* event) override;

    private slots:
        void handleSyntaxHighlightingUpdate(Theme theme);
        void onAbout();
        void handleSettingsStateChange(UiConst::SettingsMessageState state);
        void handleContextMenuDeleteAction(ResultsTable* resultTable);

    private: // NOLINT(readability-redundant-access-specifiers)
        // Build UI internal
        void buildUi();
        void setupBrowseTab();
        void setupAddTab();
        void setupSettingsTab();
        void setupIconInfo();
        void viewResource(int id, const QString &title, const QString &path);
        void showContextMenu(const QPoint &pos, int id, const QString &title, const QString &path);
        void loadResourceContent(int id, const QString &path, PlainTextEdit* viewSourceTextEdit);
        static PlainTextEdit* createResourceTextEdit(QWidget* parent);
        void setupHighlighter(PlainTextEdit* viewSourceTextEdit);
        static void removeSelectedRowsFromTable(ResultsTable* table,
                                                const QModelIndexList &selectedRows);
        static sqlite3_int64 extractIdFromRow(ResultsTable* resultTable, int row);
        void runUpdate(const QString &filePath);
        static qint64 getCurrentPid();

#if defined(Q_OS_WIN)
        void handleWindowsUpdate(const QString &filePath);
#elif defined(Q_OS_LINUX)
        void handleLinuxUpdate(const QString &filePath);
#endif

        NotesAppCore* m_core{};
        AppController* m_appController{};

        // Widgets
        QTabWidget* m_tabWidget{};
        BrowseTabWidget* m_browseTab{};
        AddTabWidget* m_addTab{};
        SettingsTabWidget* m_settingsTab{};

        // Browse Tab

        // Add Tab
        CppHighlighter* m_cppHighlighter{};
        CodeEditorLineHighlighter* m_lineHighlighter{};

        // Settings Tab

        InfoCornerWidget* m_infoWidget{};

        UiConst::SettingsMessageState m_settingsMessageState{UiConst::SettingsMessageState::none};

        QProgressDialog* m_progressDialog{};
};
