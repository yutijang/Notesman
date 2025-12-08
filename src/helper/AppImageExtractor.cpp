#include "AppImageExtractor.hpp"
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <sys/stat.h>
#include <unistd.h> // for ::chmod

bool AppImageExtractor::extractUpdater(const QString &appImagePath, const QString &outputPath,
                                       int extractTimeoutMs) {
    // basic checks
    QFileInfo imgInfo(appImagePath);
    if (!imgInfo.exists() || !imgInfo.isFile()) {
        qWarning() << "AppImage does not exist:" << appImagePath;
        return false;
    }

    // ensure executable bit on downloaded AppImage (best-effort)
    {
        const QByteArray pathUtf8 = appImagePath.toLocal8Bit();
        if (::chmod(pathUtf8.constData(), 0755) != 0) {
            qWarning() << "Warning: chmod on AppImage failed (may still run):" << appImagePath;
        }
    }

    // prepare unique workdir under temp
    const QString tmpBase = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tmpBase.isEmpty()) {
        qWarning() << "Temp location not available";
        return false;
    }
    const QString workDir = tmpBase + QLatin1String("/squashfs-") +
                            QString::number(QDateTime::currentMSecsSinceEpoch()) +
                            QLatin1String("-") +
                            QString::number(QRandomGenerator::global()->generate() & 0xffff);
    if (!QDir().mkpath(workDir)) {
        qWarning() << "Cannot create workDir:" << workDir;
        return false;
    }

    // start extraction: <appImage> --appimage-extract, working dir = workDir
    QProcess proc;
    proc.setWorkingDirectory(workDir);
    QStringList args;
    args << QLatin1String("--appimage-extract");

    proc.start(appImagePath, args);
    if (!proc.waitForStarted(5000)) {
        qWarning() << "Failed to start AppImage extraction process for" << appImagePath
                   << "cwd=" << workDir << "error:" << proc.error();
        QDir(workDir).removeRecursively();
        return false;
    }

    // wait longer: default timeout increased (caller can override)
    if (!proc.waitForFinished(extractTimeoutMs > 0 ? extractTimeoutMs : 30000)) {
        qWarning() << "AppImage extraction timed out (killing). stdout:"
                   << proc.readAllStandardOutput() << "stderr:" << proc.readAllStandardError();
        proc.kill();
        proc.waitForFinished(2000);
        QDir(workDir).removeRecursively();
        return false;
    }

    // check exit status
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        qWarning() << "AppImage extractor failed. exitCode:" << proc.exitCode()
                   << "stdout:" << proc.readAllStandardOutput()
                   << "stderr:" << proc.readAllStandardError();
        QDir(workDir).removeRecursively();
        return false;
    }

    // Now search for candidate updater binaries under workDir/squashfs-root/usr/bin (and also
    // usr/local/bin)
    const QStringList candidateDirs = {workDir + QLatin1String("/squashfs-root/usr/bin"),
                                       workDir + QLatin1String("/squashfs-root/usr/local/bin"),
                                       workDir + QLatin1String("/squashfs-root/bin"),
                                       workDir + QLatin1String("/squashfs-root/usr/sbin")};

    QString foundPath;
    for (const QString &d : candidateDirs) {
        QDir cd(d);
        if (!cd.exists()) { continue; }
        QDirIterator it(d, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString f = it.next();
            const QString base = QFileInfo(f).fileName().toLower();
            if (base.contains(QLatin1String("updater")) ||
                base.contains(QLatin1String("notesman-updater"))) {
                foundPath = f;
                break;
            }
        }
        if (!foundPath.isEmpty()) { break; }
    }

    // fallback: try any file named exactly "updater_linux" or "updater"
    if (foundPath.isEmpty()) {
        QList<QString> fallbackPaths = {
            workDir + QLatin1String("/squashfs-root/usr/bin/updater_linux"),
            workDir + QLatin1String("/squashfs-root/usr/bin/updater"),
            workDir + QLatin1String("/squashfs-root/usr/bin/notesman-updater")};
        for (const QString &p : fallbackPaths) {
            if (QFile::exists(p)) {
                foundPath = p;
                break;
            }
        }
    }

    if (foundPath.isEmpty()) {
        qWarning() << "extractUpdater: updater binary not found inside extracted AppImage under"
                   << workDir;
        // optionally list some files for debugging
        QStringList sample;
        for (const QString &d : candidateDirs) {
            if (QDir(d).exists()) {
                QDirIterator it(d, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
                int cnt = 0;
                while (it.hasNext() && cnt++ < 20) { sample << it.next(); }
            }
        }
        qWarning() << "Some sample files:" << sample;
        QDir(workDir).removeRecursively();
        return false;
    }

    // copy found file to outputPath (overwrite)
    QFile::remove(outputPath); // ignore result
    if (!QFile::copy(foundPath, outputPath)) {
        qWarning() << "Failed to copy extracted updater from" << foundPath << "to" << outputPath;
        QDir(workDir).removeRecursively();
        return false;
    }

    // set exec permission on copied file
    QFile::Permissions perms = QFile::permissions(outputPath);
    QFile::setPermissions(outputPath, perms | QFileDevice::ExeOwner);

    // final verification
    QFileInfo finalInfo(outputPath);
    bool ok = finalInfo.exists() && finalInfo.isFile() &&
              (QFile::permissions(outputPath) & QFileDevice::ExeOwner);
    if (!ok) {
        qWarning() << "Copied updater not executable or missing at" << outputPath;
        QDir(workDir).removeRecursively();
        return false;
    }

    // cleanup extracted tree
    QDir(workDir).removeRecursively();
    qInfo() << "extractUpdater: success. extracted=" << foundPath << "->" << outputPath;
    return true;
}
