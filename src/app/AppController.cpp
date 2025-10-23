#include <fstream>
#include <ios>
#include <memory>
#include <filesystem>
#include <exception>
#include <QObject>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QString>
#include <QMessageBox>
#include <QTranslator>
#include "AppController.hpp"
#include "MainWindow.hpp"
#include "database_checker.hpp"
#include "sqldb_raii.hpp"
#include "resource_repository.hpp"
#include "file_repository.hpp"
#include "text_content_repository.hpp"
#include "tag_repository.hpp"
#include "file_service.hpp"
#include "resource_service.hpp"
#include "NotesAppCore.hpp"

AppController::AppController(QObject* parent) : QObject(parent) {}

void AppController::initializeCore() {
    const QString dbFullPath = QCoreApplication::applicationDirPath() + "/data.db";
    const std::filesystem::path dbPath = dbFullPath.toStdString();

    if (!std::filesystem::exists(dbPath)) {
        emit errorOccurred(QObject::tr("Database file not found. Please create it first."));
        emit requestDatabaseCreation();
        return;
    }

    std::ifstream f(dbPath, std::ios::binary);
    if (!f.is_open()) {
        emit errorOccurred(QObject::tr("Failed to open database file."));
        return;
    }

    const int len{16};
    std::array<char, len> header{};
    f.read(header.data(), header.size());
    if (!f || f.gcount() != len) {
        emit errorOccurred(QObject::tr("Failed to read database header."));
        return;
    }

    std::string_view headerView(header.data(), header.size());
    if (!headerView.starts_with("SQLite format 3")) { // C++20+
        emit errorOccurred(QObject::tr("Invalid database file."));
        emit requestDatabaseCreation();
        return;
    }

    try {
        m_db = std::make_unique<SQLiteDB>(dbPath.string());

        verifyDatabase();
        loadSettings();

        m_resRepo = std::make_unique<ResourceRepository>(*m_db);
        m_fileRepo = std::make_unique<FileRepository>(*m_db);
        m_textRepo = std::make_unique<TextContentRepository>(*m_db);
        m_tagRepo = std::make_unique<TagRepository>(*m_db);
        m_fileService = std::make_unique<FileService>(*m_db, *m_fileRepo, *m_resRepo);
        m_resService = std::make_unique<ResourceService>(*m_db, *m_resRepo, *m_fileRepo,
                                                         *m_textRepo, *m_tagRepo, *m_fileService);
        m_core = std::make_unique<NotesAppCore>(*m_db, *m_resRepo, *m_fileRepo, *m_textRepo,
                                                *m_tagRepo, *m_fileService, *m_resService);

        emit coreReady(m_core.get());

    } catch (const std::exception &ex) { emit errorOccurred(QString::fromStdString(ex.what())); }
}

void AppController::setupConnections(MainWindow &mainWindow, AppController &controller) {
    QObject::connect(&mainWindow, &MainWindow::requestDatabaseInit, &controller,
                     &AppController::initializeCore, Qt::UniqueConnection);

    QObject::connect(&controller, &AppController::requestDatabaseCreation, &mainWindow,
                     &MainWindow::onDatabaseCreationRequested, Qt::UniqueConnection);

    QObject::connect(&mainWindow, &MainWindow::requestDatabaseCreation, &controller,
                     &AppController::createDatabase, Qt::UniqueConnection);

    QObject::connect(&controller, &AppController::coreReady, &mainWindow, &MainWindow::setCore,
                     Qt::UniqueConnection);

    QObject::connect(&controller, &AppController::errorOccurred, &mainWindow,
                     &MainWindow::showError, Qt::UniqueConnection);

    QObject::connect(&controller, &AppController::infoMessage, &mainWindow, &MainWindow::showInfo,
                     Qt::UniqueConnection);
}

void AppController::createDatabase() {
    const QString dbPath = QCoreApplication::applicationDirPath() + "/data.db";
    const QString schemaResourcePath = ":/database/notes_manager_schema.sql";

    // Đọc nội dung file .sql
    QFile schemaFile(schemaResourcePath);
    if (!schemaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred(tr("Schema resource not found: %1").arg(schemaResourcePath));

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
                                .arg((dbPtr != nullptr) ? sqlite3_errmsg(dbPtr) : "unknown");
        if (dbPtr != nullptr) { sqlite3_close_v2(dbPtr); }
        emit errorOccurred(msg);

        return;
    }

    // Thực thi schema
    char* errMsg = nullptr;
    rc = sqlite3_exec(dbPtr, schemaData.constData(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        QString msg =
            tr("Failed to execute schema: %1").arg((errMsg != nullptr) ? errMsg : "unknown");
        sqlite3_free(errMsg);
        sqlite3_close_v2(dbPtr);
        emit errorOccurred(msg);

        return;
    }

    sqlite3_close_v2(dbPtr);
    emit infoMessage(tr("Database created successfully at %1").arg(dbPath));

    initializeCore();
}

void AppController::verifyDatabase() {
    std::vector<std::string> issues;
    DatabaseChecker checker(*m_db);

    bool result = checker.checkIntegrity(issues);
    if (!result) {
        QString msg = "Database integrity check failed:\n";
        for (const auto &e : issues) { msg += QString::fromStdString(e) + "\n"; }

        QMessageBox::critical(nullptr, "Database Corruption", msg);
    }
    // else {
    //     QMessageBox::information(nullptr, "Database OK", "Database integrity is valid.");
    // }
}

void AppController::loadSettings() {
    const std::filesystem::path configPath =
        std::filesystem::path(QCoreApplication::applicationDirPath().toStdString()) / "config.ini";

    if (!std::filesystem::exists(configPath)) {
        m_settings = std::make_unique<AppSettings>();
        emit settingsLoaded(*m_settings);
        return;
    }

    m_settings = std::make_unique<AppSettings>();
    if (!m_settings->load(configPath)) {
        if (!m_settings->save(configPath)) {
            emit settingsLoaded(*m_settings);
            QMessageBox::critical(nullptr, "Error", "Can not save config file");
        }
    }
}

void AppController::saveSettings() {
    const std::filesystem::path configPath =
        std::filesystem::path(QCoreApplication::applicationDirPath().toStdString()) / "config.ini";
    if (m_settings) {
        if (!m_settings->save(configPath)) {
            QMessageBox::critical(nullptr, "Error", "Can not save config file");
        }
    }
}

void AppController::updateSettings(const AppSettings &newSettings) {
    if (!m_settings) {
        m_settings = std::make_unique<AppSettings>();
    } else {
        *m_settings = newSettings;
    }

    saveSettings();
}

const AppSettings* AppController::settings() const noexcept {
    return m_settings.get();
}

void AppController::applyLanguage(Language lang) {
    if (m_translator) { qApp->removeTranslator(m_translator.get()); }

    if (lang == Language::vietnamese) {
        m_translator = std::make_unique<QTranslator>();
        if (m_translator->load(":/i18n/app_vi.qm")) {
            qApp->installTranslator(m_translator.get());
        } else {
            m_translator.reset();
        }
    } else {
        m_translator.reset();
    }

    QEvent ev(QEvent::LanguageChange);
    QCoreApplication::sendEvent(qApp, &ev);

    emit languageChanged();
}

void AppController::applyTheme(Theme theme) {
    QString qssPath;

    switch (theme) {
        case Theme::light: qssPath = ":/themes/light.qss"; break;
        case Theme::dark : qssPath = ":/themes/dark.qss"; break;
    }

    QFile qssFile(qssPath);
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        const QString styleSheet = QString::fromUtf8(qssFile.readAll());
        qApp->setStyleSheet(styleSheet);
        qssFile.close();
    } else {
        // Nếu không mở được file theme, fallback sang mặc định
        qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setStyleSheet("");
    }

    // ✅ Áp dụng theme tô màu cho code editor
    if (m_mainWindow != nullptr) { m_mainWindow->applySyntaxHighlightingTheme(theme); }
}

void AppController::setMainWindow(MainWindow* window) {
    m_mainWindow = window;
}
