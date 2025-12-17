#pragma once

#include <QString>

struct UpdateInfoSummary {
        [[nodiscard]] bool isValid() const {
            return !assetName.isEmpty() && !assetDownloadURL.isEmpty() && !assetHash.isEmpty();
        }

        QString releaseName;
        QString assetName;
        QString assetDownloadURL;
        QString assetHash;
};
