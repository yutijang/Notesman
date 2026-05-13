#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>
#include <filesystem>

class CorePaths {
  public:
    static QString appDataDir() noexcept {
#ifdef Q_OS_WIN
        return QCoreApplication::applicationDirPath();
#else
        QString const dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir{}.mkpath(dir);
        return dir;
#endif
    }

    static QString databaseFile() noexcept {
        return appDataDir() + "/data.db";
    }

    static QString configFile() noexcept {
        return appDataDir() + "/config.ini";
    }

    static QString resolveResourcePath(QString const& path,
                                       std::filesystem::path const& resourceDir) {
        if (path.isEmpty()) {
            return path;
        }

        QFileInfo const fi(path);
        if (fi.isAbsolute()) {
            return fi.absoluteFilePath();
        }

        QDir const baseDir(QString::fromStdString(resourceDir.lexically_normal().string()));

        QString relativePath = path;
        if (relativePath.startsWith("resources/") || relativePath.startsWith("resources\\")) {
            relativePath = relativePath.mid(static_cast<int>(QString("resources/").length()));
        }

        return baseDir.absoluteFilePath(relativePath);
    }
};
