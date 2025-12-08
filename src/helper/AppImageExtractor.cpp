#include "AppImageExtractor.hpp"
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QFileInfo>

bool AppImageExtractor::extractUpdater(const QString &appImagePath, const QString &outputPath,
                                       int extractTimeoutMs) {
    // 1) basic checks
    QFileInfo imgInfo(appImagePath);
    if (!imgInfo.exists() || !imgInfo.isFile()) { return false; }

    // 2) prepare a workdir for extraction (use system temp)
    const QString tmpBase = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tmpBase.isEmpty()) { return false; }

    // use a unique dir name to avoid collision
    const QString workDir = tmpBase + QLatin1String("/squashfs-") +
                            QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir().mkpath(workDir);

    // 3) call AppImage with --appimage-extract (it creates squashfs-root under cwd)
    //    We run it with working directory = workDir
    QProcess proc;
    proc.setWorkingDirectory(workDir);
    // use the appImagePath as program, pass --appimage-extract
    // Some AppImages require executable bit; ensure we can execute it.
    if (!QFile::exists(appImagePath)) {
        QDir(workDir).removeRecursively();
        return false;
    }

    // Make sure the download file is executable (best-effort)
    QFile::setPermissions(appImagePath, QFile::permissions(appImagePath) | QFileDevice::ExeOwner);

    QStringList args;
    args << QLatin1String("--appimage-extract");

    proc.start(appImagePath, args);
    if (!proc.waitForStarted(2000)) {
        QDir(workDir).removeRecursively();
        return false;
    }

    if (!proc.waitForFinished(extractTimeoutMs)) {
        // timed out
        proc.kill();
        proc.waitForFinished(2000);
        QDir(workDir).removeRecursively();
        return false;
    }

    // 4) expected extracted path: workDir + "/squashfs-root/usr/bin/notesman-updater"
    const QString extractedPath =
        workDir + QLatin1String("/squashfs-root/usr/bin/notesman-updater");
    QFileInfo exInfo(extractedPath);
    if (!exInfo.exists() || !exInfo.isFile()) {
        // cleanup
        QDir(workDir).removeRecursively();
        return false;
    }

    // 5) copy to outputPath (overwrite if exists)
    QFile::remove(outputPath); // ignore result
    if (!QFile::copy(extractedPath, outputPath)) {
        QDir(workDir).removeRecursively();
        return false;
    }

    // 6) set executable permission on outputPath
    QFile::setPermissions(outputPath, QFile::permissions(outputPath) | QFileDevice::ExeOwner);

    // 7) cleanup extracted dir
    QDir(workDir).removeRecursively();

    // success
    return QFile::exists(outputPath) &&
           ((QFile::permissions(outputPath) & static_cast<int>(QFileDevice::ExeOwner != 0U)) != 0U);
}
