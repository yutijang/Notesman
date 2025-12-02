#pragma once

#include <QString>
#include <QDir>
#include <QCoreApplication>

namespace CorePaths {
    inline QString appRootDir() {
        return QCoreApplication::applicationDirPath();
    }

    inline QString databaseFile() {
        return appRootDir() + "/data.db";
    }

    inline QString logsDir() {
        const QString dir = appRootDir() + "/logs";
        QDir{}.mkpath(dir);
        return dir;
    }

    inline QString mainLogFile() {
        return logsDir() + "/app.log";
    }
} // namespace CorePaths
