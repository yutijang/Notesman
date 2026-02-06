#ifdef Q_OS_WIN
    #include <windows.h>
#endif

#include <algorithm>
#include <memory>
#include <fstream>
#include <ios>
#include <filesystem>
#include <array>
#include <exception>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStringView>
#include <QMessageBox>
#include <QTimer>
#include <QDir>
#include <QObject>
#include <Qt>
#include <QStringList>

#include "AppInitializer.hpp"
#include "MainWindow.hpp"
#include "AppController.hpp"
#include "FontLoader.hpp"
#include "sqldb_raii.hpp"
#include "resource_repository.hpp"
#include "file_repository.hpp"
#include "text_content_repository.hpp"
#include "tag_repository.hpp"
#include "file_service.hpp"
#include "resource_service.hpp"
#include "NotesAppCore.hpp"
#include "database_checker.hpp"
#include "DialogUtils.hpp"
#include "database_creator.hpp"
#include "CorePaths.hpp"
#include "app_version.hpp"
#include "SettingsManager.hpp"
#include "AppSettings.hpp"
#include "Logger.hpp"
#include "file_text_content_repository.hpp"
#include "schema_version.hpp"
#include "url_repository.hpp"
#include "url_service.hpp"

namespace {
    constexpr auto SERVER_NAME = "Notesman_InstanceLock";
    constexpr int TIMEWAIT{100};
    constexpr int K_EPUB_CACHE_EXPIRED{7};
} // namespace

bool AppInitializer::ensureSingleInstance() {
    QLocalSocket socket;
    socket.connectToServer(SERVER_NAME);

    if (socket.waitForConnected(TIMEWAIT)) {
#ifdef _WIN32
        AllowSetForegroundWindow(ASFW_ANY);
#endif
        socket.write("activate");
        socket.flush();
        socket.waitForBytesWritten(TIMEWAIT);
        socket.disconnectFromServer();

        return false;
    }

    QLocalServer::removeServer(SERVER_NAME);
    m_localServer = std::make_unique<QLocalServer>();

    QObject::connect(m_localServer.get(), &QLocalServer::newConnection, this,
                     &AppInitializer::onSecondInstanceMessage);

    m_localServer->listen(SERVER_NAME);

    return true;
}

void AppInitializer::onSecondInstanceMessage() {
    if (!m_localServer || !m_localServer->hasPendingConnections()) { return; }

    std::unique_ptr<QLocalSocket> client(m_localServer->nextPendingConnection());
    if (!client) { return; }

    if (!client->waitForReadyRead(TIMEWAIT)) { return; }

    const QByteArray msg = client->readAll();

    if (msg == "activate" && m_mainWindow) {
#ifdef _WIN32
        // kích hoạt cửa sổ
        HWND hwnd =
            reinterpret_cast<HWND>(m_mainWindow->winId()); // NOLINT(performance-no-int-to-ptr)
        if (IsIconic(hwnd) != 0) {
            ShowWindow(hwnd, SW_RESTORE);                  // khôi phục nếu đang minimize
        }
        SetForegroundWindow(hwnd);
#else
        // Linux
        if (m_mainWindow->isMinimized()) { m_mainWindow->showNormal(); }
        m_mainWindow->raise();
        m_mainWindow->activateWindow();
#endif
    }
}

void AppInitializer::run() {
    FontLoader::loadCustomFontOnce();

    m_controller = std::make_unique<AppController>();
    m_mainWindow = std::make_unique<MainWindow>();

    m_controller->setMainWindow(m_mainWindow.get());
    m_mainWindow->setAppController(m_controller.get());

    // Phải tạo connect trước khi emit signal coreReady trong initializeCore()
    // vì Qt không queue lại signal cũ
    setupInitializerConnections();

    const auto initCheck = initializeCore();
    if (initCheck != InitFailureReason::ok) {
        switch (initCheck) {
            case InitFailureReason::ok: // clang(-Wswitch)
            case InitFailureReason::userCancelled: break;
            case InitFailureReason::openFailed   : Log::err("Failed to open database file."); break;
            case InitFailureReason::readFailed   : Log::err("Failed to read database header."); break;
            case InitFailureReason::getNullDBVersion:
                Log::err("Error get Database version.");
                break;
            case InitFailureReason::verifyDBCorrupted: Log::err("Database corrupted."); break;
            case InitFailureReason::dbOutdated       : Log::err("Database version is outdated."); break;
        }

        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        return;
    }

    m_controller->loadSettings();

    const AppSettings* settings = m_controller->settings();
    if (settings != nullptr) {
        m_controller->applyLanguage(settings->language());
        m_controller->applyTheme(settings->theme());
    }

    m_mainWindow->show();

    QTimer::singleShot(0, [this]() { m_controller->oauthManager(); });

    checkUpdateFlag();

    cleanupOldEpubCache(K_EPUB_CACHE_EXPIRED);
}

AppInitializer::InitFailureReason AppInitializer::initializeCore() {
    const QString dbFullPath = CorePaths::databaseFile();
    const std::filesystem::path dbPath = dbFullPath.toStdString();

    if (!std::filesystem::exists(dbPath)) {
        const auto reply =
            DialogUtils::showQuestion(m_mainWindow.get(), tr("Database Missing"),
                                      tr("No database found. Would you like to create a new one?"));

        if (reply == QMessageBox::Yes) {
            createDatabase();
        } else {
            return InitFailureReason::userCancelled;
        }
    }

    std::ifstream dbFile(dbPath, std::ios::binary);
    if (!dbFile.is_open()) {
        DialogUtils::showError(m_mainWindow.get(), tr("Error"),
                               tr("Failed to open database file."));
        return InitFailureReason::openFailed;
    }

    constexpr const int len{16};
    std::array<char, len> header{};
    dbFile.read(header.data(), header.size());
    if (!dbFile || dbFile.gcount() != len) {
        DialogUtils::showError(m_mainWindow.get(), tr("Error"),
                               tr("Failed to read database header."));
        return InitFailureReason::readFailed;
    }

    std::string_view headerView(header.data(), header.size());
    if (!headerView.starts_with("SQLite format 3")) { // C++20+
        DialogUtils::showError(m_mainWindow.get(), tr("Error"), tr("Invalid database file."));

        const auto reply =
            DialogUtils::showQuestion(m_mainWindow.get(), tr("Invalid Database"),
                                      tr("The existing file is not a valid SQLite database.\n"
                                         "Would you like to recreate it?"));

        if (reply == QMessageBox::Yes) {
            createDatabase();
        } else {
            return InitFailureReason::userCancelled;
        }
    }

    try {
        m_db = std::make_unique<SQLiteDB>(dbPath.string());

        const auto dbVerifyResult = verifyDatabase();
        if (dbVerifyResult != InitFailureReason::ok) { return dbVerifyResult; }

        m_resRepo = std::make_unique<ResourceRepository>(*m_db);
        m_fileRepo = std::make_unique<FileRepository>(*m_db);
        m_textRepo = std::make_unique<TextContentRepository>(*m_db);
        m_fileTextRepo = std::make_unique<FileTextContentRepository>(*m_db);
        m_tagRepo = std::make_unique<TagRepository>(*m_db);
        m_urlRepo = std::make_unique<UrlRepository>(*m_db);
        m_fileService = std::make_unique<FileService>(*m_fileRepo, *m_resRepo, *m_fileTextRepo);
        m_urlService = std::make_unique<UrlService>(*m_urlRepo, *m_resRepo);
        m_resService = std::make_unique<ResourceService>(
            *m_db, *m_resRepo, *m_fileRepo, *m_textRepo, *m_tagRepo, *m_fileService, *m_urlService);
        m_core = std::make_unique<NotesAppCore>(*m_textRepo, *m_fileService, *m_urlService,
                                                *m_resService);

        if (m_controller) {
            m_controller->loadSettings();
            m_controller->setCore(m_core.get());
        }

        Q_EMIT coreReady(m_core.get());

    } catch (const std::exception &ex) {
        Log::err(ex.what());
        DialogUtils::showError(m_mainWindow.get(), tr("Error"), QString::fromStdString(ex.what()));
    }

    return InitFailureReason::ok;
}

void AppInitializer::createDatabase() {
    const QString dbPath = CorePaths::databaseFile();
    const QString schemaResourcePath = ":/schema/notes_manager_schema.sql";

    // Đọc nội dung file .sql
    QFile schemaFile(schemaResourcePath);
    if (!schemaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Log::err("Schema resource not found: {}", schemaResourcePath.toStdString());
        DialogUtils::showError(m_mainWindow.get(), tr("Error"),
                               tr("Schema resource not found: %1").arg(schemaResourcePath));

        return;
    }

    const std::string schemaSql = schemaFile.readAll().toStdString();
    const std::string dbPathUtf8 = dbPath.toUtf8().constData();

    if (std::string error; !DatabaseCreator::create(dbPathUtf8, schemaSql, error)) {
        Log::err("Error create database: {}", error);
        DialogUtils::showError(m_mainWindow.get(), tr("Error"), QString::fromStdString(error));
        return;
    }

    DialogUtils::showInfo(m_mainWindow.get(), tr("Information"),
                          tr("Database created successfully at %1").arg(dbPath));

    initializeCore();
}

AppInitializer::InitFailureReason AppInitializer::verifyDatabase() {
    DatabaseChecker checker(*m_db);

    if (std::vector<std::string> issues; !checker.checkIntegrity(issues)) {
        QString msg = tr("Database integrity check failed:\n");
        for (const auto &e : issues) { msg += QString::fromStdString(e) + "\n"; }

        Log::err(msg.toStdString());
        DialogUtils::showError(
            m_mainWindow.get(), tr("Database Corrupted"),
            tr("The application has detected that the database file is corrupted or damaged.\n\n"
               "Error details:\n"
               "%1\n\n"
               "To fix this problem:\n"
               "1. Close the application completely\n"
               "2. Delete the file 'data.db'\n"
               "3. Restart the application\n\n"
               "Note: You will lose all local data."
               "If you have important information, please make a backup of data.db before "
               "deleting.")
                .arg(msg));

        return InitFailureReason::verifyDBCorrupted;
    }

    const auto verOpt = checker.getDBVersion();
    if (!verOpt.has_value()) { return InitFailureReason::getNullDBVersion; }

    if (const int currentVersion = *verOpt; currentVersion < app::meta::SCHEMA_VERSION) {
        Log::err("Database version is outdated, current verison: {}, required version: {}",
                 currentVersion, app::meta::SCHEMA_VERSION);
        DialogUtils::showInfo(m_mainWindow.get(), tr("Incompatible Database"),
                              tr("Database version (%1) is outdated (Required: %2).\n\n"
                                 "To fix this problem:\n"
                                 "1. Close the application completely\n"
                                 "2. Delete the file 'data.db'\n"
                                 "3. Restart the application\n\n"
                                 "Note: You will lose all local data. "
                                 "If you have important information, please make a backup of "
                                 "data.db before deleting.")
                                  .arg(currentVersion)
                                  .arg(app::meta::SCHEMA_VERSION));

        return InitFailureReason::dbOutdated;
    }

    return InitFailureReason::ok;
}

void AppInitializer::setupInitializerConnections() {
    // B. Initializer báo cáo Core đã sẵn sàng -> Window nhận
    QObject::connect(this, &AppInitializer::coreReady, m_mainWindow.get(), &MainWindow::setCore,
                     Qt::UniqueConnection);

    QObject::connect(m_controller.get(), &AppController::closeConnectDBRequestForward, this,
                     &AppInitializer::closeDatabaseConnection);
    QObject::connect(m_controller.get(), &AppController::reconnectDBRequestForward, this,
                     &AppInitializer::reinitializeDatabaseConnection);

    QObject::connect(this, &AppInitializer::dbClosed, m_controller.get(),
                     &AppController::dbClosedForward);
}

void AppInitializer::closeDatabaseConnection(bool isUpload) {
    if (m_db) {
        m_db->close();

        Q_EMIT dbClosed(isUpload);
    }
}

void AppInitializer::reinitializeDatabaseConnection() {
    if (!m_db) { return; }

    try {
        const std::string filename = m_db->getFilename();
        m_db->open(filename);

        if (const auto dbVerify = verifyDatabase(); dbVerify != InitFailureReason::ok) {
            if (dbVerify == InitFailureReason::getNullDBVersion) {
                Log::err("Error get Database version.");
            } else if (dbVerify == InitFailureReason::verifyDBCorrupted) {
                Log::err("Database corrupted.");
            } else if (dbVerify == InitFailureReason::verifyDBCorrupted) {
                Log::err("Database version is outdated.");
            }

            return;
        }

        Q_EMIT dbOpened();
    } catch (const std::exception &ex) { Log::fatal("Fatal error: {}", ex.what()); }
}

void AppInitializer::checkUpdateFlag() {
    const QStringList args = QApplication::arguments();

    if (bool isUpdateDone =
            std::ranges::any_of(args, [](const QString &arg) { return arg == "--update-done"; });
        !isUpdateDone) {
        return;
    }

#if defined(Q_OS_WIN)
    if (args.size() < 5) { // NOLINT(readability-magic-numbers)
        return;
    }
    // arguments received from stage 2
    // argv[1] = --update-done
    // argv[2] = PID stage2 (process called: temp_update/updater.exxe)
    // argv[3] = temp_update dir path
    // argv[4] = zip path

    const auto updaterPID = static_cast<DWORD>(args[2].toULongLong());
    waitForProcessExitAsync(updaterPID, [this, args]() {
        handleUpdateCleanup(args);
        saveETagOnUpdateSuccess();
    });
#elif defined(Q_OS_LINUX)
    if (args.size() < 4) { return; }

    namespace fs = std::filesystem;

    const fs::path oldAppPath(args[2].toStdString());
    const fs::path selfPath = fs::read_symlink("/proc/self/exe");

    if (std::error_code ec; fs::exists(oldAppPath) && fs::weakly_canonical(oldAppPath, ec) !=
                                                          fs::weakly_canonical(selfPath, ec)) {
        fs::remove(oldAppPath);
    }

    const fs::path binPath(args[3].toStdString());
    if (fs::exists(binPath)) { fs::remove(binPath); }

    displayNotiUpdateComplete();
    saveETagOnUpdateSuccess();
#endif
}

void AppInitializer::saveETagOnUpdateSuccess() {
    auto &settings = SettingsManager::instance();

    const QString pendingETag = settings.get("update/pending_etag").toString();
    const QString pendingVersion = settings.get("update/pending_version").toString();

    // Chỉ xác nhận thành công nếu version khớp
    if (!pendingETag.isEmpty() && !pendingVersion.isEmpty() &&
        pendingVersion == app::meta::VERSION) {
        settings.set("update/applied_etag", pendingETag);
        settings.set("update/applied_version", pendingVersion);

        Log::info("Update applied successfully. Version: {}, ETag: {}",
                  pendingVersion.toStdString(), pendingETag.toStdString());
    } else {
        Log::warn("Update mismatch: Expected {}, but binary version is {}",
                  pendingVersion.toStdString(), QString(app::meta::VERSION).toStdString());
    }

    // Dù thành công hay thất bại, pending đều phải bị xóa
    settings.remove("update/pending_etag");
    settings.remove("update/pending_version");
}

void AppInitializer::handleUpdateCleanup(const QStringList &args) {
    std::filesystem::path tempDirPath(args[3].toStdWString());
    std::error_code ec;

    if (std::filesystem::exists(tempDirPath)) {
        std::filesystem::remove_all(tempDirPath, ec);
        if (ec) {
            Log::err("Failed to remove temp_update: {}", ec.message());
            DialogUtils::showError(
                m_mainWindow.get(), tr("Error"),
                tr("Failed to remove temp_update: %1").arg(QString::fromStdString(ec.message())));
            return;
        }
    }

    std::filesystem::path zipPath(args[4].toStdWString());
    if (std::filesystem::exists(zipPath)) { std::filesystem::remove(zipPath, ec); }

    // defer dialog until UI is ready
    displayNotiUpdateComplete();
}

void AppInitializer::displayNotiUpdateComplete() {
    QTimer::singleShot(0, m_mainWindow.get(), [this]() {
        const QString kLinkColor = (m_controller->isDarkTheme()) ? "#4FC3F7" : "#0000EE";
        DialogUtils::showInfo(m_mainWindow.get(), tr("Update complete"),
                              tr("Application has been updated successfully.<br>"
                                 "Version: v%1<br>"
                                 "Changelog: <a href=\"%2\" style=\"color: %3; "
                                 "text-decoration: underline;\">%2</a>")
                                  .arg(app::meta::VERSION)
                                  .arg(app::meta::WEBSITE)
                                  .arg(kLinkColor),
                              true);
    });
}

#ifdef Q_OS_WIN
void AppInitializer::waitForProcessExitAsync(DWORD pid, const std::function<void()> &onExited) {
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (h == nullptr) {
        onExited();
        return;
    }

    auto* timer = new QTimer(this);
    timer->setInterval(100);                      // NOLINT(readability-magic-numbers)

    QObject::connect(timer, &QTimer::timeout, this, [timer, h, onExited]() {
        DWORD result = WaitForSingleObject(h, 0); // non-blocking
        if (result == WAIT_OBJECT_0) {
            timer->stop();
            timer->deleteLater();
            CloseHandle(h);
            onExited();
        }
    });

    timer->start();
}
#endif

void AppInitializer::cleanupOldEpubCache(int days) {
    const QDir dir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/epub");
    const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

    const QDateTime now = QDateTime::currentDateTime();

    for (const auto &fi : entries) {
        if (fi.lastModified().daysTo(now) > days) {
            QDir(fi.absoluteFilePath()).removeRecursively();
        }
    }
}
