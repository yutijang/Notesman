#ifdef Q_OS_WIN
    #include <windows.h>
#endif

#include <memory>
#include <fstream>
#include <ios>
#include <filesystem>
#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStringView>
#include <QMessageBox>
#include <QTimer>
#include <QDir>

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

namespace {
    constexpr auto SERVER_NAME = "Notesman_InstanceLock";
    constexpr int TIMEWAIT{100};
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

    setupInitializerConnections();

    m_mainWindow->setAppController(m_controller.get());

    m_controller->loadSettings();

    const AppSettings* settings = m_controller->settings();
    if (settings != nullptr) {
        m_controller->applyLanguage(settings->language());
        m_controller->applyTheme(settings->theme());
    }

    m_mainWindow->show();

    QTimer::singleShot(0, [this]() { m_controller->oauthManager(); });

    checkUpdateFlag();
}

void AppInitializer::initializeCore() {
    const QString dbFullPath = CorePaths::databaseFile();
    const std::filesystem::path dbPath = dbFullPath.toStdString();

    if (!std::filesystem::exists(dbPath)) {
        const auto reply =
            DialogUtils::showQuestion(m_mainWindow.get(), tr("Database Missing"),
                                      tr("No database found. Would you like to create a new one?"));

        if (reply == QMessageBox::Yes) {
            createDatabase();
        } else {
            if (m_mainWindow != nullptr) { m_mainWindow->close(); }
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        }

        return;
    }

    std::ifstream dbFile(dbPath, std::ios::binary);
    if (!dbFile.is_open()) {
        DialogUtils::showError(m_mainWindow.get(), tr("Error"),
                               tr("Failed to open database file."));
        return;
    }

    const int len{16};
    std::array<char, len> header{};
    dbFile.read(header.data(), header.size());
    if (!dbFile || dbFile.gcount() != len) {
        DialogUtils::showError(m_mainWindow.get(), tr("Error"),
                               tr("Failed to read database header."));
        return;
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
            if (m_mainWindow != nullptr) { m_mainWindow->close(); }
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
        }

        return;
    }

    try {
        m_db = std::make_unique<SQLiteDB>(dbPath.string());

        verifyDatabase();

        m_resRepo = std::make_unique<ResourceRepository>(*m_db);
        m_fileRepo = std::make_unique<FileRepository>(*m_db);
        m_textRepo = std::make_unique<TextContentRepository>(*m_db);
        m_tagRepo = std::make_unique<TagRepository>(*m_db);
        m_fileService = std::make_unique<FileService>(*m_db, *m_fileRepo, *m_resRepo);
        m_resService = std::make_unique<ResourceService>(*m_db, *m_resRepo, *m_fileRepo,
                                                         *m_textRepo, *m_tagRepo, *m_fileService);
        m_core = std::make_unique<NotesAppCore>(*m_db, *m_resRepo, *m_fileRepo, *m_textRepo,
                                                *m_tagRepo, *m_fileService, *m_resService);

        if (m_controller) {
            m_controller->loadSettings();
            m_controller->setCore(m_core.get());
        }

        emit coreReady(m_core.get());

    } catch (const std::exception &ex) {
        DialogUtils::showError(m_mainWindow.get(), tr("Error"), QString::fromStdString(ex.what()));
    }
}

void AppInitializer::createDatabase() {
    const QString dbPath = CorePaths::databaseFile();
    const QString schemaResourcePath = ":/schema/notes_manager_schema.sql";

    // Đọc nội dung file .sql
    QFile schemaFile(schemaResourcePath);
    if (!schemaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        DialogUtils::showError(m_mainWindow.get(), tr("Error"),
                               tr("Schema resource not found: %1").arg(schemaResourcePath));

        return;
    }

    const std::string schemaSql = schemaFile.readAll().toStdString();
    const std::string dbPathUtf8 = dbPath.toUtf8().constData();

    std::string error;

    if (!DatabaseCreator::create(dbPathUtf8, schemaSql, error)) {
        DialogUtils::showError(m_mainWindow.get(), tr("Error"), QString::fromStdString(error));
        return;
    }

    DialogUtils::showInfo(m_mainWindow.get(), tr("Information"),
                          tr("Database created successfully at %1").arg(dbPath));

    initializeCore();
}

void AppInitializer::verifyDatabase() {
    std::vector<std::string> issues;
    DatabaseChecker checker(*m_db);

    bool result = checker.checkIntegrity(issues);
    if (!result) {
        QString msg = tr("Database integrity check failed:\n");
        for (const auto &e : issues) { msg += QString::fromStdString(e) + "\n"; }

        DialogUtils::showError(m_mainWindow.get(), tr("Database Corruption"), msg);
    }
}

void AppInitializer::setupInitializerConnections() {
    // A. Window yêu cầu khởi tạo -> Initializer thực hiện
    QObject::connect(m_mainWindow.get(), &MainWindow::requestDatabaseInit, this,
                     &AppInitializer::initializeCore, Qt::UniqueConnection);

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
        qDebug() << "connect to DB closed";

        emit dbClosed(isUpload);
    }
}

void AppInitializer::reinitializeDatabaseConnection() {
    if (!m_db) { return; }

    try {
        const std::string filename = m_db->getFilename();
        m_db->open(filename);

        verifyDatabase();

        qDebug() << "connect to DB opened";

        emit dbOpened();
    } catch (const std::exception &ex) { qDebug() << "Fatal error: " << ex.what(); }
}

void AppInitializer::checkUpdateFlag() {
    const QStringList args = QApplication::arguments();

#if defined(Q_OS_WIN)
    if (args.size() >= 5 && args[1] == "--update-done") { // NOLINT(readability-magic-numbers)

        // arguments received from stage 2
        // argv[1] = --update-done
        // argv[2] = PID stage2 (process called: temp_update/updater.exxe)
        // argv[3] = temp_update dir path
        // argv[4] = zip path

        const auto updaterPID = static_cast<DWORD>(args[2].toULongLong());
        waitForProcessExitAsync(updaterPID, [this, args]() { handleUpdateCleanup(args); });
        saveETagOnUpdateSuccess();
    }
#elif defined(Q_OS_LINUX)
    if (args.size() >= 4 && args[1] == "--update-done") {
        namespace fs = std::filesystem;

        const fs::path oldAppPath(args[2].toStdString());
        const fs::path selfPath = fs::read_symlink("/proc/self/exe");

        std::error_code ec;
        if (fs::exists(oldAppPath) &&
            fs::weakly_canonical(oldAppPath, ec) != fs::weakly_canonical(selfPath, ec)) {
            fs::remove(oldAppPath);
        }

        const fs::path binPath(args[3].toStdString());
        if (fs::exists(binPath)) { fs::remove(binPath); }

        displayNotiUpdateComplete();
        saveETagonUpdateSuccess();
    }
#endif
}

void AppInitializer::saveETagOnUpdateSuccess() {
    QSettings settings(UiConst::SETTINGS_ORG, UiConst::SETTINGS_APP);

    const auto checkedETag = settings.value("update/pending_etag").toString();
    if (!checkedETag.isEmpty()) {
        settings.setValue("update/applied_etag", checkedETag);
        settings.setValue("update/applied_version", app::meta::VERSION);
        settings.remove("update/pending_etag");
    }
}

void AppInitializer::handleUpdateCleanup(const QStringList &args) {
    std::filesystem::path tempDirPath(args[3].toStdWString());
    std::error_code ec;

    if (std::filesystem::exists(tempDirPath)) {
        std::filesystem::remove_all(tempDirPath, ec);
        if (ec) {
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
