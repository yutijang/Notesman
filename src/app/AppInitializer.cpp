#ifdef _WIN32
#    include <windows.h>
#endif

#include <memory>
#include <fstream>
#include <ios>
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
#include "UiConstants.hpp"

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
}

void AppInitializer::initializeCore() {
    const QString dbFullPath = QCoreApplication::applicationDirPath() + "/data.db";
    const std::filesystem::path dbPath = dbFullPath.toStdString();

    if (!std::filesystem::exists(dbPath)) {
        const auto reply = QMessageBox::question(
            nullptr, QObject::tr("Database Missing"),
            QObject::tr("No database found. Would you like to create a new one?"),
            QMessageBox::Yes | QMessageBox::No);

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
        emit errorOccurred(QObject::tr("Failed to open database file."),
                           UiConst::MessageType::error);
        return;
    }

    const int len{16};
    std::array<char, len> header{};
    dbFile.read(header.data(), header.size());
    if (!dbFile || dbFile.gcount() != len) {
        emit errorOccurred(QObject::tr("Failed to read database header."),
                           UiConst::MessageType::error);
        return;
    }

    std::string_view headerView(header.data(), header.size());
    if (!headerView.starts_with("SQLite format 3")) { // C++20+
        emit errorOccurred(QObject::tr("Invalid database file."), UiConst::MessageType::error);

        const auto reply =
            QMessageBox::question(nullptr, QObject::tr("Invalid Database"),
                                  QObject::tr("The existing file is not a valid SQLite database.\n"
                                              "Would you like to recreate it?"),
                                  QMessageBox::Yes | QMessageBox::No);

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
        emit errorOccurred(QString::fromStdString(ex.what()), UiConst::MessageType::error);
    }
}

void AppInitializer::createDatabase() {
    const QString dbPath = QCoreApplication::applicationDirPath() + "/data.db";
    const QString schemaResourcePath = ":/schema/notes_manager_schema.sql";

    // Đọc nội dung file .sql
    QFile schemaFile(schemaResourcePath);
    if (!schemaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred(tr("Schema resource not found: %1").arg(schemaResourcePath),
                           UiConst::MessageType::error);

        return;
    }

    const QByteArray schemaData = schemaFile.readAll();
    schemaFile.close();

    // Đảm bảo thư mục tồn tại
    QDir().mkpath(QFileInfo(dbPath).absolutePath());

    sqlite3* dbPtr = nullptr;
    int rc = sqlite3_open_v2(dbPath.toStdString().c_str(), &dbPtr,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        const QString msg = tr("Cannot create database: %1")
                                .arg((dbPtr != nullptr) ? sqlite3_errmsg(dbPtr) : tr("unknown"));
        if (dbPtr != nullptr) { sqlite3_close_v2(dbPtr); }
        emit errorOccurred(msg, UiConst::MessageType::error);

        return;
    }

    // Thực thi schema
    char* errMsg = nullptr;
    rc = sqlite3_exec(dbPtr, schemaData.constData(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        QString msg =
            tr("Failed to execute schema: %1").arg((errMsg != nullptr) ? errMsg : tr("unknown"));
        sqlite3_free(errMsg);
        sqlite3_close_v2(dbPtr);
        emit errorOccurred(msg, UiConst::MessageType::error);

        return;
    }

    sqlite3_close_v2(dbPtr);
    emit infoMessage(tr("Database created successfully at %1").arg(dbPath),
                     UiConst::MessageType::info);

    initializeCore();
}

void AppInitializer::verifyDatabase() {
    std::vector<std::string> issues;
    DatabaseChecker checker(*m_db);

    bool result = checker.checkIntegrity(issues);
    if (!result) {
        QString msg = tr("Database integrity check failed:\n");
        for (const auto &e : issues) { msg += QString::fromStdString(e) + "\n"; }

        QMessageBox::critical(nullptr, tr("Database Corruption"), msg);
    }
}

void AppInitializer::setupInitializerConnections() {
    // A. Window yêu cầu khởi tạo -> Initializer thực hiện
    QObject::connect(m_mainWindow.get(), &MainWindow::requestDatabaseInit, this,
                     &AppInitializer::initializeCore, Qt::UniqueConnection);

    // B. Initializer báo cáo Core đã sẵn sàng -> Window nhận
    QObject::connect(this, &AppInitializer::coreReady, m_mainWindow.get(), &MainWindow::setCore,
                     Qt::UniqueConnection);

    // C. Initializer báo cáo lỗi/thông tin -> Window nhận
    QObject::connect(this, &AppInitializer::errorOccurred, m_mainWindow.get(),
                     &MainWindow::showNoti, Qt::UniqueConnection);
    QObject::connect(this, &AppInitializer::infoMessage, m_mainWindow.get(), &MainWindow::showNoti,
                     Qt::UniqueConnection);
}
