#pragma once

#include <QString>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

class CorePaths {
    public:
        static QString appDataDir() noexcept {
#ifdef Q_OS_WIN
            return QCoreApplication::applicationDirPath();
#else
            const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QDir{}.mkpath(dir);
            return dir;
#endif
        }

        static QString databaseFile() noexcept { return appDataDir() + "/data.db"; }

        static QString configFile() noexcept { return appDataDir() + "/config.ini"; }
};
