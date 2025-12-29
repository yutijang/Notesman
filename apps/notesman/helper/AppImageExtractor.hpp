#pragma once

#include <QString>

struct AppImageExtractor {
        // Extract notesman-updater (path inside AppImage: usr/bin/notesman-updater)
        // from appImagePath to outputPath (e.g. "/tmp/notesman-updater").
        // Returns true on success.
        static bool extractUpdater(const QString &appImagePath, const QString &outputPath);
};
