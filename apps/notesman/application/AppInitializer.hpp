#pragma once

#include <QObject>
#include <cstdint>
#include <unordered_map>

#ifdef Q_OS_WIN
#include <functional>
#endif

#include "application/AppController.hpp"
#include "application/NotesAppCore.hpp"
#include "application/NotesCoreFactory.hpp"
#include "core/db/sqldb_raii.hpp"
#include "core/repository/file_repository.hpp"
#include "core/repository/file_text_content_repository.hpp"
#include "core/repository/resource_repository.hpp"
#include "core/repository/tag_repository.hpp"
#include "core/repository/text_content_repository.hpp"
#include "core/repository/url_repository.hpp"
#include "core/service/file_service.hpp"
#include "core/service/resource_service.hpp"
#include "core/service/url_service.hpp"
#include "gui/MainWindow.hpp"

#include <QLocalServer>
#include <QLocalSocket>
#include <QString>
#include <QStringList>
#include <memory>

class QDialog;

class AppInitializer final : public QObject {
    Q_OBJECT

  public:
    AppInitializer() = default;
    ~AppInitializer() override = default;

    bool ensureSingleInstance();
    void run();
    void closeDatabaseConnection(bool isUpload);
    void reinitializeDatabaseConnection();

    void registerViewerDialog(std::int64_t resourceId, QDialog* dlg);
    void unregisterViewerDialog(std::int64_t resourceId);

  Q_SIGNALS:
    void coreReady(NotesAppCore* core);
    void dbClosed(bool isUpload); // true = for upload, false = for download
    void dbOpened();

    void cleanupEpubCacheRequest(int days);
    void cleanupMDCacheRequest(int days);

  private: // NOLINT(readability-redundant-access-specifiers)
    using InitFailureReason = NotesCoreFactory::InitFailureReason;

    InitFailureReason initializeCore();
    InitFailureReason verifyDatabase();

    void setupInitializerConnections();
    void checkUpdateFlag();
    void onSecondInstanceMessage();

    void handleUpdateCleanup(QStringList const& args);
    void displayNotiUpdateComplete();
    static void saveETagOnUpdateSuccess();

#ifdef Q_OS_WIN
    void waitForProcessExitAsync(DWORD pid, std::function<void()> const& onExited);
#endif

    std::unique_ptr<SQLiteDB> m_db;
    std::unique_ptr<ResourceRepository> m_resRepo;
    std::unique_ptr<FileRepository> m_fileRepo;
    std::unique_ptr<TextContentRepository> m_textRepo;
    std::unique_ptr<FileTextContentRepository> m_fileTextRepo;
    std::unique_ptr<TagRepository> m_tagRepo;
    std::unique_ptr<FileService> m_fileService;
    std::unique_ptr<UrlRepository> m_urlRepo;
    std::unique_ptr<UrlService> m_urlService;
    std::unique_ptr<ResourceService> m_resService;
    std::unique_ptr<MainWindow> m_mainWindow;
    std::unique_ptr<AppController> m_controller;
    std::unique_ptr<QLocalServer> m_localServer;
    std::unique_ptr<NotesAppCore> m_core;

    std::unordered_map<std::int64_t, QDialog*> m_activeViewers;
};
