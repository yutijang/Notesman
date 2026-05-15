#include "application/PackerLauncher.hpp"

#include <limits>

#ifdef Q_OS_WIN
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <QEventLoop>
#include <QProcess>
#include <csignal>
#include <ctime>
#endif

#include "application/AppUIApplier.hpp"
#include "application/CoreErrorReporter.hpp"
#include "application/NotesCoreFactory.hpp"
#include "application/ResourceViewerFactory.hpp"
#include "common/logger/Logger.hpp"
#include "common/viewer_pack/ViewerPackHeader.hpp"
#include "common/viewer_pack/ViewerPackReader.hpp"
#include "core/model/model.hpp"
#include "gui/Settings/AppSettings.hpp"
#include "gui/UiConstants.hpp"
#include "gui/Viewer/ResourceViewService.hpp"
#include "gui/Viewer/ResourceViewerDialog.hpp"
#include "helper/CorePaths.hpp"
#include "helper/FontLoader.hpp"
#include "helper/IpcConstants.hpp"

#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QObject>
#include <QString>
#include <QTranslator>
#include <QtTypes>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <sqlite3.h>
#include <string_view>
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
    auto const* const uuid = header.uuid;
    auto const theme = static_cast<UiConst::Theme>(header.themeMode);
    auto const language = static_cast<UiConst::Language>(header.language);

    // IPC check — phải trước khi init core/DB
    QString const serverName = IpcNames::packerServer(uuid);
    {
        QLocalSocket probe;
        probe.connectToServer(serverName);
        if (probe.waitForConnected(IpcNames::K_IPC_TIMEOUT_MS)) {
            // Đã có process với resourceId này → báo focus rồi thoát
            probe.write("activate");
            probe.flush();
            probe.waitForBytesWritten(IpcNames::K_IPC_TIMEOUT_MS);
            probe.disconnectFromServer();
            Log::info("packer instance for resource {} already running, activating.", uuid);
            return 0;
        }
    }

    // Chưa có process nào → trở thành server cho resourceId này
    QLocalServer::removeServer(serverName);
    auto localServer = std::make_unique<QLocalServer>();
    if (!localServer->listen(serverName)) {
        Log::warn("listen failed for resource {}, retrying connect...", uuid);
        // Server vừa được process khác tạo trong khoảng thời gian race
        QLocalSocket retry;
        retry.connectToServer(serverName);
        if (retry.waitForConnected(IpcNames::K_IPC_TIMEOUT_MS)) {
            retry.write("activate");
            retry.flush();
            retry.waitForBytesWritten(IpcNames::K_IPC_TIMEOUT_MS);
            retry.disconnectFromServer();
            return 0;
        }
        // Vẫn không connect được → tiếp tục mà không có IPC server (non-fatal)
        Log::warn("IPC server unavailable for resource {}, proceeding without it.", uuid);
    }

    {
        QLocalSocket guiProbe;
        guiProbe.connectToServer(IpcNames::K_GUI_SERVER);

        if (guiProbe.waitForConnected(IpcNames::K_IPC_TIMEOUT_MS)) {
            // GUI đang chạy — query xem resourceId này có đang mở không
            QByteArray const query = QByteArray("query:") + uuid;
            guiProbe.write(query);
            guiProbe.flush();

            if (guiProbe.waitForReadyRead(IpcNames::K_IPC_TIMEOUT_MS)) {
                QByteArray const response = guiProbe.readAll();

                if (response == "found") {
                    // Viewer đang mở trong GUI — gửi activate rồi thoát
                    guiProbe.write(QByteArray("activate:") + uuid);
                    guiProbe.flush();
                    guiProbe.waitForBytesWritten(IpcNames::K_IPC_TIMEOUT_MS);
                    guiProbe.disconnectFromServer();
                    Log::info("resource {} already open in GUI, activating.", uuid);
                    return 0;
                }
            }
            guiProbe.disconnectFromServer();
        }
    }

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

    /**
     * Temporarily disabled startup auto-registration;
     * deferring to the existing manual option in Settings.
     */
    /*
    #ifdef Q_OS_WIN
        if (!FileAssociation::isUpToDate()) {
            // silent, non-fatal
            if (!FileAssociation::registerAssociation()) { Log::warn("auto-register failed."); }
        }
    #endif
    */

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

    if (res.resource.uuid != std::string_view(header.uuid)) {
        QMessageBox::warning(
            nullptr,
            QObject::tr("Invalid resource"),
            QObject::tr("The file does not match the current database.\n"
                        "The resource may have been deleted or the data has been reset."));
        return 1;
    }

    QString const title = QString::fromStdString(res.resource.title);

    // Resolve path
    QString const rawPath =
        res.filepath.has_value() ? QString::fromStdString(*res.filepath) : QString{};
    QString const absolutePath = CorePaths::resolveResourcePath(rawPath, settings.resourceDir());
    QString const url = res.url.has_value() ? QString::fromStdString(*res.url) : QString{};

    // Tạo viewer
    ResourceViewService viewService(*ctx.core);

    auto viewer = ResourceViewerFactory::create(static_cast<std::int64_t>(res.resource.id),
                                                res.resource.type,
                                                title,
                                                absolutePath,
                                                url,
                                                theme,
                                                viewService,
                                                nullptr);
    if (!viewer) {
        Log::err("failed to create viewer for resource {}.", resourceId);
        return 1;
    }

#ifdef Q_OS_LINUX
    if (viewer->usesExternalWindow()) {
        QProcess* const proc = viewer->externalProcess();
        if (proc == nullptr) {
            return 1;
        }

        QEventLoop loop;

        QObject::connect(localServer.get(), &QLocalServer::newConnection, [&localServer, proc]() {
            if (!localServer->hasPendingConnections()) [[unlikely]] {
                return;
            }
            QLocalSocket* client = localServer->nextPendingConnection();
            if (!client) [[unlikely]] {
                return;
            }

            QObject::connect(client, &QLocalSocket::readyRead, [client, proc]() {
                QByteArray const msg = client->readAll();
                if (msg != "activate") {
                    return;
                }

                qint64 const pid64 = proc->processId();
                if (pid64 <= 0 || pid64 > std::numeric_limits<pid_t>::max()) [[unlikely]] {
                    return;
                }
                // ::kill(proc->processId(), SIGUSR1);
                ::kill(static_cast<pid_t>(pid64), SIGUSR1);
                QObject::connect(
                    client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
            });
        });

        QObject::connect(proc, &QProcess::finished, &loop, &QEventLoop::quit);
        loop.exec();

        localServer->close();
        QLocalServer::removeServer(serverName);
        return 0;
    }
#endif

    // Tạo dialog + kết nối IPC signal
    auto* dlg = new ResourceViewerDialog{title, std::move(viewer), nullptr};

    // Khi nhận "activate" từ instance thứ 2 → focus dialog
    QObject::connect(localServer.get(), &QLocalServer::newConnection, [&localServer, dlg]() {
        if (!localServer->hasPendingConnections()) [[unlikely]] {
            return;
        }

        // Gắn parent = localServer → Qt delete khi disconnect
        QLocalSocket* client = localServer->nextPendingConnection();
        if (!client) [[unlikely]] {
            return;
        }

        QObject::connect(client, &QLocalSocket::readyRead, [client, dlg]() {
            QByteArray const msg = client->readAll();
            if (msg != "activate") {
                return;
            }
#if defined(Q_OS_WIN)
            HWND hwnd = reinterpret_cast<HWND>(dlg->winId()); // NOLINT(performance-no-int-to-ptr)
            if (IsIconic(hwnd) != 0) {
                ShowWindow(hwnd, SW_RESTORE);
            }
            SetForegroundWindow(hwnd);
#else
            if (dlg->isMinimized()) { dlg->showNormal(); }
            dlg->raise();
            dlg->activateWindow();
#endif
            // Auto cleanup khi disconnect
            QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        });
    });

    // Blocking exec — server sống trong scope này
    dlg->exec();

    // cleanup
    localServer->close();
    QLocalServer::removeServer(serverName);

    return 0;
}
