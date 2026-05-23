#pragma once

#include "application/ICoreErrorHandler.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <utility>

class SQLiteDB;
class ResourceRepository;
class FileRepository;
class TextContentRepository;
class FileTextContentRepository;
class TagRepository;
class UrlRepository;
class FileService;
class UrlService;
class ResourceService;
class NotesAppCore;

class NotesCoreFactory {
  public:
    enum class InitFailureReason : std::uint8_t {
        Ok,
        UserCancelled,
        OpenFailed,
        ReadFailed,
        VerifyDBCorrupted,
        GetNullDBVersion,
        DBOutdated
    };

    struct CoreContext {
        std::unique_ptr<SQLiteDB> db;
        std::unique_ptr<ResourceRepository> resRepo;
        std::unique_ptr<FileRepository> fileRepo;
        std::unique_ptr<TextContentRepository> textRepo;
        std::unique_ptr<FileTextContentRepository> fileTextRepo;
        std::unique_ptr<TagRepository> tagRepo;
        std::unique_ptr<UrlRepository> urlRepo;

        std::unique_ptr<FileService> fileService;
        std::unique_ptr<UrlService> urlService;
        std::unique_ptr<ResourceService> resService;

        std::unique_ptr<NotesAppCore> core;

        ~CoreContext();
    };

    struct CoreInitResult {
        std::unique_ptr<CoreContext> context;
        InitFailureReason reason;

        explicit CoreInitResult(InitFailureReason r) : context(nullptr), reason(r) {}

        explicit CoreInitResult(std::unique_ptr<CoreContext> ctx)
            : context(std::move(ctx)), reason(InitFailureReason::Ok) {}
    };

    static CoreInitResult createCore(std::filesystem::path const& dbPath,
                                     ICoreErrorHandler* errorHandler);

  private:
    // Tạo DB từ schema resource, báo lỗi qua errorHandler
    static bool internalCreateDatabase(std::filesystem::path const& dbPath,
                                       ICoreErrorHandler* errorHandler);

    // Kiểm tra integrity + schema version — gọi sau khi db đã mở thành công
    static InitFailureReason internalVerifyDatabase(SQLiteDB& db);
};
