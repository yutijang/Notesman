#include "NotesCoreFactory.hpp"

#include "ICoreErrorHandler.hpp"
#include "Logger.hpp"
#include "NotesAppCore.hpp"
#include "database_checker.hpp"
#include "database_creator.hpp"
#include "file_repository.hpp"
#include "file_service.hpp"
#include "file_text_content_repository.hpp"
#include "resource_repository.hpp"
#include "resource_service.hpp"
#include "schema_version.hpp"
#include "tag_repository.hpp"
#include "text_content_repository.hpp"
#include "url_repository.hpp"
#include "url_service.hpp"

#include <QFile>
#include <QIODevice>
#include <QString>
#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

NotesCoreFactory::CoreInitResult NotesCoreFactory::createCore(std::filesystem::path const& dbPath,
                                                              ICoreErrorHandler* errorHandler) {
    if (!std::filesystem::exists(dbPath)) {
        bool const wantsCreate =
            (errorHandler != nullptr) &&
            errorHandler->askQuestion("Database Missing",
                                      "No database found. Would you like to create a new one?");
        if (!wantsCreate) { return CoreInitResult(InitFailureReason::UserCancelled); }

        if (!internalCreateDatabase(dbPath, errorHandler)) {
            return CoreInitResult(InitFailureReason::OpenFailed);
        }
    }

    {
        std::ifstream dbFile(dbPath, std::ios::binary);
        if (!dbFile.is_open()) {
            if (errorHandler == nullptr) {
                errorHandler->showError("Error", "Failed to open database file.");
            }
            return CoreInitResult(InitFailureReason::OpenFailed);
        }

        constexpr int kHeaderLen{16};
        std::array<char, kHeaderLen> header{};
        dbFile.read(header.data(), header.size());

        if (!dbFile || dbFile.gcount() != kHeaderLen) {
            if (errorHandler == nullptr) {
                errorHandler->showError("Error", "Failed to read database header.");
            }
            return CoreInitResult(InitFailureReason::ReadFailed);
        }

        std::string_view const headerView(header.data(), header.size());
        if (!headerView.starts_with("SQLite format 3")) {
            bool const wantsRecreate =
                (errorHandler != nullptr) &&
                errorHandler->askQuestion(
                    "Invalid Database", "The existing file is not a valid SQLite database.\nWould "
                                        "you like to recreate it?");
            if (!wantsRecreate) { return CoreInitResult(InitFailureReason::UserCancelled); }

            if (!internalCreateDatabase(dbPath, errorHandler)) {
                return CoreInitResult(InitFailureReason::OpenFailed);
            }
        }
    }

    try {
        auto ctx = std::make_unique<CoreContext>();

        ctx->db = std::make_unique<SQLiteDB>(dbPath.string());

        auto const verifyResult = internalVerifyDatabase(*ctx->db);
        if (verifyResult != InitFailureReason::Ok) {
            if (errorHandler != nullptr) {
                switch (verifyResult) {
                    case InitFailureReason::VerifyDBCorrupted:
                        errorHandler->showError(
                            "Database Corrupted",
                            "The database file is corrupted or damaged.\n\n"
                            "To fix this problem:\n"
                            "1. Close the application completely\n"
                            "2. Delete the file 'data.db'\n"
                            "3. Restart the application\n\n"
                            "Note: You will lose all local data. "
                            "If you have important information, please make a backup of "
                            "data.db before deleting.");
                        break;
                    case InitFailureReason::DBOutdated:
                        errorHandler->showError(
                            "Incompatible Database",
                            QString("Database version is outdated (Required: %1).\n\n"
                                    "To fix this problem:\n"
                                    "1. Close the application completely\n"
                                    "2. Delete the file 'data.db'\n"
                                    "3. Restart the application\n\n"
                                    "Note: You will lose all local data. "
                                    "If you have important information, please make a backup of "
                                    "data.db before deleting.")
                                .arg(app::meta::SCHEMA_VERSION));
                        break;
                    case InitFailureReason::GetNullDBVersion:
                        errorHandler->showError("Error", "Failed to read database version.");
                        break;
                    default: break;
                }
            }
            return CoreInitResult(verifyResult);
        }

        ctx->resRepo = std::make_unique<ResourceRepository>(*ctx->db);
        ctx->fileRepo = std::make_unique<FileRepository>(*ctx->db);
        ctx->textRepo = std::make_unique<TextContentRepository>(*ctx->db);
        ctx->fileTextRepo = std::make_unique<FileTextContentRepository>(*ctx->db);
        ctx->tagRepo = std::make_unique<TagRepository>(*ctx->db);
        ctx->urlRepo = std::make_unique<UrlRepository>(*ctx->db);

        ctx->fileService =
            std::make_unique<FileService>(*ctx->fileRepo, *ctx->resRepo, *ctx->fileTextRepo);
        ctx->urlService = std::make_unique<UrlService>(*ctx->urlRepo, *ctx->resRepo);
        ctx->resService = std::make_unique<ResourceService>(*ctx->db, *ctx->resRepo, *ctx->fileRepo,
                                                            *ctx->textRepo, *ctx->tagRepo,
                                                            *ctx->fileService, *ctx->urlService);

        ctx->core = std::make_unique<NotesAppCore>(*ctx->textRepo, *ctx->fileService,
                                                   *ctx->urlService, *ctx->resService);

        return CoreInitResult(std::move(ctx));

    } catch (std::exception const& ex) {
        Log::err("NotesCoreFactory::createCore exception: {}", ex.what());
        if (errorHandler != nullptr) {
            errorHandler->showError("Error", QString::fromStdString(ex.what()));
        }
        return CoreInitResult(InitFailureReason::OpenFailed);
    }
}

bool NotesCoreFactory::internalCreateDatabase(std::filesystem::path const& dbPath,
                                              ICoreErrorHandler* errorHandler) {
    QString const schemaResourcePath = ":/schema/notes_manager_schema.sql";
    QFile schemaFile(schemaResourcePath);

    if (!schemaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString const errMsg = QString("Schema resource not found: %1").arg(schemaResourcePath);
        Log::err(errMsg.toStdString());
        if (errorHandler != nullptr) { errorHandler->showError("Error", errMsg); }
        return false;
    }

    std::string const schemaSql = schemaFile.readAll().toStdString();
    std::string const dbPathStr = dbPath.string();

    if (std::string error; !DatabaseCreator::create(dbPathStr, schemaSql, error)) {
        Log::err("Error create database: {}", error);
        if (errorHandler != nullptr) {
            errorHandler->showError("Error", QString::fromStdString(error));
            return false;
        }
    }

    if (errorHandler != nullptr) {
        errorHandler->showInfo(
            "Information",
            QString("Database created successfully at %1").arg(QString::fromStdString(dbPathStr)));
    }

    return true;
}

NotesCoreFactory::InitFailureReason NotesCoreFactory::internalVerifyDatabase(SQLiteDB& db) {
    DatabaseChecker checker(db);

    if (std::vector<std::string> issues; !checker.checkIntegrity(issues)) {
        std::string errMsg;
        for (auto const& e : issues) { errMsg += e + "\n"; }
        Log::err("Database integrity check failed:\n{}", errMsg);
        return InitFailureReason::VerifyDBCorrupted;
    }

    auto const verOpt = checker.getDBVersion();
    if (!verOpt.has_value()) {
        Log::err("Failed to read database version.");
        return InitFailureReason::GetNullDBVersion;
    }

    if (int const currentVersion = *verOpt; currentVersion < app::meta::SCHEMA_VERSION) {
        Log::err("Database version is outdated, current: {}, required: {}", currentVersion,
                 app::meta::SCHEMA_VERSION);
        return InitFailureReason::DBOutdated;
    }

    return InitFailureReason::Ok;
}
