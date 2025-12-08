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

bool AppImageExtractor::extractUpdater(const QString &appImagePath, const QString &outputPath) {
    qWarning() << "[EXTRACT] begin";

    if (!QFile::exists(appImagePath)) {
        qWarning() << "[EXTRACT] AppImage missing:" << appImagePath;
        return false;
    }

    const QString workDir =
        QDir::tempPath() + "/nm_extract_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    if (!QDir().mkpath(workDir)) {
        qWarning() << "[EXTRACT] Cannot create workdir:" << workDir;
        return false;
    }
    qWarning() << "[EXTRACT] workdir =" << workDir;

    // chmod +x
    ::chmod(appImagePath.toLocal8Bit().constData(), 0755);

    QProcess p;
    p.setWorkingDirectory(workDir);
    p.start(appImagePath, {"--appimage-extract"});
    if (!p.waitForStarted(5000)) {
        qWarning() << "[EXTRACT] Cannot start AppImage. error=" << p.errorString();
        return false;
    }
    if (!p.waitForFinished(30000)) {
        qWarning() << "[EXTRACT] Timeout. stderr=" << p.readAllStandardError();
        return false;
    }

    qWarning() << "[EXTRACT] exitCode=" << p.exitCode() << "stderr=" << p.readAllStandardError()
               << "stdout=" << p.readAllStandardOutput();

    const QString root = workDir + "/squashfs-root";
    if (!QDir(root).exists()) {
        qWarning() << "[EXTRACT] squashfs-root missing:" << root;
        return false;
    }
    qWarning() << "[EXTRACT] squashfs-root OK";

    QString updater = root + "/usr/bin/updater_linux";
    if (!QFile::exists(updater)) {
        qWarning() << "[EXTRACT] updater_linux missing. Searching...";

        QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString f = it.next();
            if (f.contains("updater", Qt::CaseInsensitive)) {
                updater = f;
                break;
            }
        }
    }

    qWarning() << "[EXTRACT] found updater candidate:" << updater;

    if (!QFile::exists(updater)) {
        qWarning() << "[EXTRACT] updater not found in extracted FS.";
        return false;
    }

    QFile::remove(outputPath);
    if (!QFile::copy(updater, outputPath)) {
        qWarning() << "[EXTRACT] copy failed:" << updater << "->" << outputPath;
        return false;
    }

    QFile::setPermissions(outputPath, QFile::permissions(outputPath) | QFileDevice::ExeOwner);

    qWarning() << "[EXTRACT] success. output =" << outputPath;
    return true;
}
