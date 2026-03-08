#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QString>

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

        static QString databaseFile() noexcept { return appDataDir() + "/data.db"; }

        static QString configFile() noexcept { return appDataDir() + "/config.ini"; }
};
