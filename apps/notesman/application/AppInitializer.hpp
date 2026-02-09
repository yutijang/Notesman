#pragma once

#include <QObject>

#ifdef Q_OS_WIN
    #include <windows.h>
    #include <functional>
#endif

#include <cstdint>
#include <memory>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>
#include <QStringList>

#include "MainWindow.hpp"
#include "AppController.hpp"
#include "NotesAppCore.hpp"
#include "sqldb_raii.hpp"
#include "resource_repository.hpp"
#include "file_repository.hpp"
#include "text_content_repository.hpp"
#include "tag_repository.hpp"
#include "file_service.hpp"
#include "resource_service.hpp"
#include "file_text_content_repository.hpp"
#include "url_repository.hpp"
#include "url_service.hpp"

class AppInitializer final : public QObject {
        Q_OBJECT

    public:
        AppInitializer() = default;
        ~AppInitializer() override = default;

        bool ensureSingleInstance();
        void run();
        void createDatabase();
        void closeDatabaseConnection(bool isUpload);
        void reinitializeDatabaseConnection();

    Q_SIGNALS:
        void coreReady(NotesAppCore* core);
        void dbClosed(bool isUpload); // true = for upload, false = for download
        void dbOpened();

        void cleanupEpubCacheRequest(int days);
        void cleanupMDCacheRequest(int days);

    private: // NOLINT(readability-redundant-access-specifiers)
        enum class InitFailureReason : std::uint8_t {
            ok,
            userCancelled,
            openFailed,
            readFailed,
            verifyDBCorrupted,
            getNullDBVersion,
            dbOutdated
        };

        InitFailureReason initializeCore();
        InitFailureReason verifyDatabase();

        void setupInitializerConnections();
        void checkUpdateFlag();
        void onSecondInstanceMessage();

        void handleUpdateCleanup(const QStringList &args);
        void displayNotiUpdateComplete();
        static void saveETagOnUpdateSuccess();

#ifdef Q_OS_WIN
        void waitForProcessExitAsync(DWORD pid, const std::function<void()> &onExited);
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
};
