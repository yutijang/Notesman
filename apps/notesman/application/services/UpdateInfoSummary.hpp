#pragma once

#include <QString>

struct UpdateInfoSummary {
    [[nodiscard]] bool isValid() const {
        return !assetName.isEmpty() && !assetDownloadURL.isEmpty() && !assetHash.isEmpty();
    }

    QString releaseName;      //> "Notesman v2025.12.0"
    QString assetName;        //> "Notesman-x64-v2025.12.0.zip"
    QString assetDownloadURL; //> "https://github.com/../.."
    QString assetHash;        //> "1af966c..."
};
