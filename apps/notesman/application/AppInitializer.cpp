#include "AppInitializer.hpp"

#ifdef Q_OS_WIN
#include <functional>
#include <windows.h>
#endif

#include "AppController.hpp"
#include "AppSettings.hpp"
#include "CorePaths.hpp"
#include "DialogUtils.hpp"
#include "FileAssociation.hpp"
#include "FontLoader.hpp"
#include "GuiCoreErrorHandler.hpp"
#include "Logger.hpp"
#include "MainWindow.hpp"
#include "NotesCoreFactory.hpp"
#include "SettingsManager.hpp"
#include "app_version.hpp"
#include "database_checker.hpp"
#include "resource_repository.hpp"
#include "schema_version.hpp"
#include "sqldb_raii.hpp"

#include <QApplication>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QObject>
#include <QStringList>
#include <QStringView>
#include <QTimer>
#include <Qt>
#include <algorithm>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
    constexpr auto K_SERVER_NAME = "Notesman_InstanceLock";
    constexpr int K_TIMEWAIT{100};
} // namespace

bool AppInitializer::ensureSingleInstance() {
    QLocalSocket socket;
    socket.connectToServer(K_SERVER_NAME);

    if (socket.waitForConnected(K_TIMEWAIT)) {
#if defined(Q_OS_WIN)
        AllowSetForegroundWindow(ASFW_ANY);
#endif
        socket.write("activate");
        socket.flush();
        socket.waitForBytesWritten(K_TIMEWAIT);
        socket.disconnectFromServer();

        return false;
    }

    QLocalServer::removeServer(K_SERVER_NAME);
    m_localServer = std::make_unique<QLocalServer>();

    QObject::connect(m_localServer.get(), &QLocalServer::newConnection, this,
                     &AppInitializer::onSecondInstanceMessage);

    m_localServer->listen(K_SERVER_NAME);

    return true;
}

void AppInitializer::onSecondInstanceMessage() {
    if (!m_localServer || !m_localServer->hasPendingConnections()) [[unlikely]] { return; }

    std::unique_ptr<QLocalSocket> client(m_localServer->nextPendingConnection());
    if (!client) [[unlikely]] { return; }

    if (!client->waitForReadyRead(K_TIMEWAIT)) [[unlikely]] { return; }

    QByteArray const msg = client->readAll();

    if (msg == "activate" && m_mainWindow) [[likely]] {
#if defined(Q_OS_WIN)
        // kích hoạt cửa sổ
        HWND hwnd =
            reinterpret_cast<HWND>(m_mainWindow->winId()); // NOLINT(performance-no-int-to-ptr)
        if (IsIconic(hwnd) != 0) {
            ShowWindow(hwnd, SW_RESTORE);                  // khôi phục nếu đang minimize
        }
        SetForegroundWindow(hwnd);
#elif defined(Q_OS_LINUX)
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

    auto const initCheck = initializeCore();
    if (initCheck != InitFailureReason::Ok) [[unlikely]] {
        switch (initCheck) {
            case InitFailureReason::Ok: // clang(-Wswitch)
            case InitFailureReason::UserCancelled: break;
            case InitFailureReason::OpenFailed   : Log::err("Failed to open database file."); break;
            case InitFailureReason::ReadFailed   : Log::err("Failed to read database header."); break;
            case InitFailureReason::GetNullDBVersion:
                Log::err("Error get Database version.");
                break;
            case InitFailureReason::VerifyDBCorrupted: Log::err("Database corrupted."); break;
            case InitFailureReason::DBOutdated       : Log::err("Database version is outdated."); break;
        }

        QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        return;
    }

    m_controller->loadSettings();

#ifdef Q_OS_WIN
    // Silent auto-repair: nếu path stale (app bị di chuyển) thì ghi lại
    if (!FileAssociation::isUpToDate()) {
        if (!FileAssociation::registerAssociation()) { Log::warn("auto-register failed."); }
    }
#endif

    AppSettings const* settings = m_controller->settings();
    if (settings != nullptr) {
        m_controller->applyLanguage(settings->language());
        m_controller->applyTheme(settings->theme());
    }

    m_mainWindow->show();

    QTimer::singleShot(0, m_controller.get(), [this]() { m_controller->oauthManager(); });

    checkUpdateFlag();

    if (settings != nullptr) {
        if (settings->isCleanupEpubCache()) {
            Q_EMIT cleanupEpubCacheRequest(settings->daysCleanupEpubCache());
        }
        if (settings->isCleanupMDCache()) {
            Q_EMIT cleanupMDCacheRequest(settings->daysCleanupMDCache());
        }
    }
}

AppInitializer::InitFailureReason AppInitializer::initializeCore() {
    QString const dbFullPath = CorePaths::databaseFile();
    std::filesystem::path const dbPath = dbFullPath.toStdString();

    GuiCoreErrorHandler handler(m_mainWindow.get());
    auto result = NotesCoreFactory::createCore(dbPath, &handler);

    if (result.reason != InitFailureReason::Ok) { return result.reason; }

    // Move ownership từ CoreContext vào members của AppInitializer
    // Lifetime gắn với AppInitializer — tồn tại suốt vòng đời ứng dụng
    auto& ctx = *result.context;
    m_db = std::move(ctx.db);
    m_resRepo = std::move(ctx.resRepo);
    m_fileRepo = std::move(ctx.fileRepo);
    m_textRepo = std::move(ctx.textRepo);
    m_fileTextRepo = std::move(ctx.fileTextRepo);
    m_tagRepo = std::move(ctx.tagRepo);
    m_urlRepo = std::move(ctx.urlRepo);
    m_fileService = std::move(ctx.fileService);
    m_urlService = std::move(ctx.urlService);
    m_resService = std::move(ctx.resService);
    m_core = std::move(ctx.core);

    if (m_controller) {
        m_controller->loadSettings();
        m_controller->setCore(m_core.get());
    }

    Q_EMIT coreReady(m_core.get());

    return InitFailureReason::Ok;
}

AppInitializer::InitFailureReason AppInitializer::verifyDatabase() {
    DatabaseChecker checker(*m_db);

    if (std::vector<std::string> issues; !checker.checkIntegrity(issues)) {
        QString msg = tr("Database integrity check failed:\n");
        for (auto const& e : issues) { msg += QString::fromStdString(e) + "\n"; }

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

        return InitFailureReason::VerifyDBCorrupted;
    }

    auto const verOpt = checker.getDBVersion();
    if (!verOpt.has_value()) { return InitFailureReason::GetNullDBVersion; }

    if (int const currentVersion = *verOpt; currentVersion < app::meta::SCHEMA_VERSION) {
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

        return InitFailureReason::DBOutdated;
    }

    return InitFailureReason::Ok;
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

    QObject::connect(this, &AppInitializer::cleanupEpubCacheRequest, m_controller.get(),
                     &AppController::cleanupOldEpubCache);
    QObject::connect(this, &AppInitializer::cleanupMDCacheRequest, m_controller.get(),
                     &AppController::cleanupOldMarkdownCache);
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
        std::string const filename = m_db->getFilename();
        m_db->open(filename);

        if (auto const dbVerify = verifyDatabase(); dbVerify != InitFailureReason::Ok) {
            if (dbVerify == InitFailureReason::GetNullDBVersion) {
                Log::err("Error get Database version.");
            } else if (dbVerify == InitFailureReason::VerifyDBCorrupted) {
                Log::err("Database corrupted.");
            } else if (dbVerify == InitFailureReason::VerifyDBCorrupted) {
                Log::err("Database version is outdated.");
            }

            return;
        }

        Q_EMIT dbOpened();
    } catch (std::exception const& ex) { Log::fatal("Fatal error: {}", ex.what()); }
}

void AppInitializer::checkUpdateFlag() {
    QStringList const args = QApplication::arguments();

    if (bool isUpdateDone =
            std::ranges::any_of(args, [](QString const& arg) { return arg == "--update-done"; });
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

    auto const updaterPID = static_cast<DWORD>(args[2].toULongLong());
    waitForProcessExitAsync(updaterPID, [this, args]() {
        handleUpdateCleanup(args);
        saveETagOnUpdateSuccess();
    });
#elif defined(Q_OS_LINUX)
    if (args.size() < 4) { return; }

    namespace fs = std::filesystem;

    fs::path const oldAppPath(args[2].toStdString());
    fs::path const selfPath = fs::read_symlink("/proc/self/exe");

    if (std::error_code ec; fs::exists(oldAppPath) && fs::weakly_canonical(oldAppPath, ec) !=
                                                          fs::weakly_canonical(selfPath, ec)) {
        fs::remove(oldAppPath);
    }

    fs::path const binPath(args[3].toStdString());
    if (fs::exists(binPath)) { fs::remove(binPath); }

    displayNotiUpdateComplete();
    saveETagOnUpdateSuccess();
#endif
}

void AppInitializer::saveETagOnUpdateSuccess() {
    auto& settings = SettingsManager::instance();

    QString const pendingETag = settings.get("update/pending_etag").toString();
    QString const pendingVersion = settings.get("update/pending_version").toString();

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

void AppInitializer::handleUpdateCleanup(QStringList const& args) {
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
        QString const kLinkColor = (m_controller->isDarkTheme()) ? "#4FC3F7" : "#0000EE";
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
void AppInitializer::waitForProcessExitAsync(DWORD pid, std::function<void()> const& onExited) {
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
