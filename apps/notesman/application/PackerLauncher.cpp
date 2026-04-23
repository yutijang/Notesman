#include "PackerLauncher.hpp"

#include "AppSettings.hpp"
#include "AppUIApplier.hpp"
#include "CoreErrorReporter.hpp"
#include "CorePaths.hpp"
#include "FontLoader.hpp"
#include "Logger.hpp"
#include "NotesCoreFactory.hpp"
#include "ResourceViewService.hpp"
#include "ResourceViewerDialog.hpp"
#include "ResourceViewerFactory.hpp"
#include "UiConstants.hpp"
#include "ViewerPackHeader.hpp"
#include "ViewerPackReader.hpp"
#include "model.hpp"

#include <QFileInfo>
#include <QString>
#include <QTranslator>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <sqlite3.h>
#include <utility>

int PackerLauncher::run(QString const& packerFilePath) {
    // Đọc và validate .rvpk header
    auto readerResult = ViewerPackReader::read(packerFilePath.toStdString());
    if (!readerResult.has_value()) {
        Log::err("failed to read .rvpk file: {}", packerFilePath.toStdString());
        return 1;
    }

    ViewerPackHeader const& header = readerResult->header();
    auto const resourceId = static_cast<sqlite3_int64>(header.resourceId);
    auto const theme = static_cast<UiConst::Theme>(header.themeMode);
    auto const language = static_cast<UiConst::Language>(header.language);

    // Apply theme + language + font custom
    std::unique_ptr<QTranslator> translator;
    AppUI::applyLanguage(language, translator);
    AppUI::applyTheme(theme);
    FontLoader::loadCustomFontOnce();

    // Load settings để lấy resourceDir
    AppSettings settings;
    std::filesystem::path const configPath = CorePaths::configFile().toStdString();
    if (!settings.load(configPath)) {
        Log::warn("failed to load config, using default resourceDir.");
    }

    // Khởi tạo core
    // CoreErrorReporter: headless — askQuestion luôn false, lỗi ghi log
    CoreErrorReporter reporter;
    std::filesystem::path const dbPath = CorePaths::databaseFile().toStdString();
    auto coreResult = NotesCoreFactory::createCore(dbPath, &reporter);

    if (coreResult.reason != NotesCoreFactory::InitFailureReason::Ok) {
        Log::err("core initialization failed.");
        return 1;
    }

    // CoreContext alive cùng scope với dlg.exec() — đảm bảo lifetime toàn bộ stack
    auto& ctx = *coreResult.context;

    // Lấy resource từ core
    auto fullResourceOpt = ctx.core->getFullResource(resourceId, false);
    if (!fullResourceOpt.has_value()) {
        Log::err("resource {} not found in database.", resourceId);
        return 1;
    }

    FullResource const& res = *fullResourceOpt;

    QString const title = QString::fromStdString(res.resource.title);

    // Resolve path
    QString const rawPath =
        res.filepath.has_value() ? QString::fromStdString(*res.filepath) : QString{};
    QString const absolutePath = CorePaths::resolveResourcePath(rawPath, settings.resourceDir());
    QString const url = res.url.has_value() ? QString::fromStdString(*res.url) : QString{};

    // Tạo viewer
    ResourceViewService viewService(*ctx.core);

    auto viewer =
        ResourceViewerFactory::create(static_cast<std::int64_t>(res.resource.id), res.resource.type,
                                      title, absolutePath, url, theme, viewService, nullptr);
    if (!viewer) {
        Log::err("failed to create viewer for resource {}.", resourceId);
        return 1;
    }

    // Hiển thị dialog ---
    // CoreContext, viewService, translator đều alive cho đến khi dialog đóng
    auto* dlg = new ResourceViewerDialog{title, std::move(viewer), nullptr};
    dlg->exec();

    return 0;
}
