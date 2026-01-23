#pragma once

#include <optional>
#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QtTypes>
#include <QAnyStringView>
#include <QStringView>

#include "UpdateInfoSummary.hpp"

class UpdateManager final : public QObject {
        Q_OBJECT

    public:
        explicit UpdateManager(QObject* parent = nullptr);
        ~UpdateManager() override = default;

        struct UpdateInfo {
                // for display (level release)
                QString releaseName;  // key: name
                QString tagName;      // key: tag_name, aka remote version
                QString releaseNotes; // key: body

                // for download (level assets)
                QString assetName;        // key: name
                QString assetDownloadURL; // key: browser_download_url
                QString assetHash;        // key: digest
                qint64 assetSize{};       // key: size

                [[nodiscard]] bool isValid() const {
                    return !assetName.isEmpty() && !assetDownloadURL.isEmpty() &&
                           !assetHash.isEmpty() && assetSize > 0;
                }
        };

        // Kiểm tra cập nhật từ URL server
        void checkForUpdates(const QString &versionCheckUrl);

    signals:
        void updateAvailable(const UpdateInfoSummary &infoSummary);
        void noUpdateAvailable();
        void updateCheckFailed(const QString &error);

    private slots:
        void onVersionReplyFinished(QNetworkReply* reply);

    private: // NOLINT(readability-redundant-access-specifiers)
        static std::optional<UpdateInfo> findAssetInfo(const QJsonDocument &qJDoc);
        static QStringView normalizeVersionQt(QStringView version);
        static int compareVersionsQt(QAnyStringView vLocal, QAnyStringView vRemote);
        static QString extractHash(const QString &digest);

        static UpdateInfoSummary updateInfoToSummary(const UpdateInfo &updateInfo);

        QNetworkAccessManager m_networkManager;
};
