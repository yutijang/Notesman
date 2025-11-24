#pragma once

#include <QString>

struct UpdateInfoSummary {
        QString releaseName;
        QString assetName;
        QString assetDownloadURL;
        QString assetHash;

        [[nodiscard]] bool isValid() const {
            return !assetName.isEmpty() && !assetDownloadURL.isEmpty() && !assetHash.isEmpty();
        }
};
