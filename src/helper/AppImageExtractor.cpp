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
    if (!QFile::exists(appImagePath)) { return false; }

    const QString workDir =
        QDir::tempPath() + "/nm_extract_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    if (!QDir().mkpath(workDir)) { return false; }

    ::chmod(appImagePath.toLocal8Bit().constData(), 0755);

    QProcess p;
    p.setWorkingDirectory(workDir);
    p.start(appImagePath, {"--appimage-extract"});
    if (!p.waitForStarted(5000)) { return false; }
    if (!p.waitForFinished(30000)) { return false; }

    const QString root = workDir + "/squashfs-root";
    if (!QDir(root).exists()) { return false; }

    QString updater = root + "/usr/bin/updater_linux";
    if (!QFile::exists(updater)) {
        QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString f = it.next();
            if (f.contains("updater", Qt::CaseInsensitive)) {
                updater = f;
                break;
            }
        }
    }

    if (!QFile::exists(updater)) { return false; }

    QFile::remove(outputPath);
    if (!QFile::copy(updater, outputPath)) { return false; }

    QFile::setPermissions(outputPath, QFile::permissions(outputPath) | QFileDevice::ExeOwner);

    return true;
}
