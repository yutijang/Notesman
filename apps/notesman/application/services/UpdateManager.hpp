#pragma once

#include "UpdateInfoSummary.hpp"

#include <QAnyStringView>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringView>
#include <QtTypes>
#include <optional>

class QNetworkReply;

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
            return !assetName.isEmpty() && !assetDownloadURL.isEmpty() && !assetHash.isEmpty() &&
                   assetSize > 0;
        }
    };

    // Kiểm tra cập nhật từ URL server
    void checkForUpdates(QString const& versionCheckUrl);

  Q_SIGNALS:
    void updateAvailable(UpdateInfoSummary const& infoSummary);
    void noUpdateAvailable();
    void updateCheckFailed(QString const& error);

  private: // NOLINT(readability-redundant-access-specifiers)
    static std::optional<UpdateInfo> findAssetInfo(QJsonDocument const& qJDoc);
    static QStringView normalizeVersionQt(QStringView version);
    static int compareVersionsQt(QAnyStringView vLocal, QAnyStringView vRemote);
    static QString extractHash(QString const& digest);

    static UpdateInfoSummary updateInfoToSummary(UpdateInfo const& updateInfo);

    void onVersionReplyFinished(QNetworkReply* reply);

    QNetworkAccessManager m_networkManager;
};
