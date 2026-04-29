#include "UpdateManager.hpp"

#include "Logger.hpp"
#include "SettingsManager.hpp"
#include "UiConstants.hpp"
#include "UpdateInfoSummary.hpp"
#include "app_version.hpp"

#include <QAnyStringView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkInformation>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QStringView>
#include <QTimer>
#include <QUrl>
#include <QVersionNumber>
#include <Qt>
#include <optional>

UpdateManager::UpdateManager(QObject* parent) : QObject(parent) {}

void UpdateManager::checkForUpdates(QString const& versionCheckUrl) {
    if (auto* netInfo = QNetworkInformation::instance()) {
        if (netInfo->reachability() == QNetworkInformation::Reachability::Disconnected) {
            Log::info("No internet connection.");
            Q_EMIT updateCheckFailed(tr("No internet connection."));
            return;
        }
    }

    QUrl url(versionCheckUrl);
    QNetworkRequest request(url);
    request.setTransferTimeout(UiConst::NOTI_TIMEOUT5);
    QString const userAgent = QStringLiteral("%1/%2 (Contact: %3)")
                                  .arg(app::meta::NAME)
                                  .arg(app::meta::VERSION)
                                  .arg(app::meta::WEBSITE);
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");

    auto& qSettings = SettingsManager::instance();

    // Check for rollback
    QString const appliedVersion = qSettings.get("update/applied_version").toString();
    if (!appliedVersion.isEmpty() && appliedVersion != app::meta::VERSION) {
        int const compare = compareVersionsQt(app::meta::VERSION, appliedVersion);
        if (compare < 0) {
            qSettings.remove("update/applied_etag");
            qSettings.remove("update/applied_version");
            qSettings.remove("update/pending_etag");
            qSettings.remove("update/pending_version");

            QString const msg = QStringLiteral("Rollback detected: local=%1, applied=%2")
                                    .arg(app::meta::VERSION)
                                    .arg(appliedVersion);

            Log::warn(msg.toStdString());
        }
    }

    QString const pendingVersion = qSettings.get("update/pending_version").toString();
    if (!pendingVersion.isEmpty()) {
        int const compare = compareVersionsQt(app::meta::VERSION, pendingVersion);
        if (compare < 0) {
            // Có pending update chưa apply
            Log::info("Pending update detected but not applied yet.");
        }
    }

    QString const appliedETag = qSettings.get("update/applied_etag").toString();
    if (!appliedETag.isEmpty()) { request.setRawHeader("If-None-Match", appliedETag.toUtf8()); }

    QNetworkReply* reply = m_networkManager.get(request);

    auto* timeoutTimer = new QTimer(reply);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(UiConst::NOTI_TIMEOUT5);
    QObject::connect(timeoutTimer, &QTimer::timeout, reply, [this, reply]() {
        if (reply->isRunning()) {
            reply->abort();
            Q_EMIT updateCheckFailed(
                tr("Network connection too slow (timeout exceeded 5 seconds)."));
        }
    });
    timeoutTimer->start();

    QObject::connect(reply, &QNetworkReply::finished, this,
                     [this, reply]() { onVersionReplyFinished(reply); });
}

void UpdateManager::onVersionReplyFinished(QNetworkReply* reply) {
    if (reply == nullptr) { return; }

    auto const status = reply->error();
    if (status != QNetworkReply::NoError) {
        Q_EMIT updateCheckFailed(reply->errorString());
        reply->deleteLater();
        return;
    }

    constexpr int kNotModiCode{304};
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == kNotModiCode) {
        Q_EMIT noUpdateAvailable();
        reply->deleteLater();
        return;
    }

    QByteArray const pendingETag = reply->rawHeader("ETag");
    QByteArray const data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    auto const jsonDoc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
        Log::err("Invalid update JSON: {}", parseError.errorString().toStdString());
        Q_EMIT updateCheckFailed(tr("Invalid update JSON: %1").arg(parseError.errorString()));
        return;
    }

    auto const releaseObj = jsonDoc.object();
    QString const latestVersion = releaseObj.value("tag_name").toString();
    auto const removeVer = normalizeVersionQt(latestVersion);

    auto& qSettings = SettingsManager::instance();

    int const checkForUpdate = compareVersionsQt(app::meta::VERSION, removeVer);
    // Không có cập nhật mới
    if (checkForUpdate >= 0) {
        qSettings.remove("update/pending_etag");
        qSettings.remove("update/pending_version");

        Q_EMIT noUpdateAvailable();

        return;
    }

    // Bắt đầu logic cập nhật
    if (pendingETag.isEmpty()) {
        Log::info("Missing ETag");
        Q_EMIT updateCheckFailed("Missing ETag");
        return;
    }

    auto updateInfo = findAssetInfo(jsonDoc);
    if (!updateInfo.has_value() || !updateInfo->isValid()) {
        Log::info("Error gather update info");
        Q_EMIT updateCheckFailed(tr("Error gather update info"));
        return;
    }

    qSettings.set("update/pending_etag", pendingETag);
    qSettings.set("update/pending_version", removeVer.toString());

    updateInfo->tagName = latestVersion;

    Q_EMIT updateAvailable(updateInfoToSummary(*updateInfo));
}

std::optional<UpdateManager::UpdateInfo> UpdateManager::findAssetInfo(QJsonDocument const& qJDoc) {
#if defined(Q_OS_WIN)
    constexpr auto preferredExt = "zip";
#elif defined(Q_OS_LINUX)
    constexpr auto preferredExt = "AppImage";
#elif defined(Q_OS_MAC)
    constexpr auto preferredExt = "dmg";
#else
    constexpr auto preferredExt = "";
#endif

    UpdateInfo downloadInfo{};

    auto const releaseObj = qJDoc.object();
    downloadInfo.releaseName = releaseObj.value("name").toString();
    downloadInfo.releaseNotes = releaseObj.value("body").toString();

    QJsonArray assets = qJDoc["assets"].toArray();

    int assetIdx{};
    for (; assetIdx < assets.size(); ++assetIdx) {
        auto const obj = assets[assetIdx].toObject();
        auto const assetName = obj["name"].toString();
        if (assetName.endsWith(preferredExt, Qt::CaseInsensitive)) {
            downloadInfo.assetName = assetName;
            break;
        }
    }

    // Nếu không tìm thấy asset, thông tin không toàn vẹn, trả về thông tin rỗng mặc định
    if (downloadInfo.assetName.isEmpty()) { return std::nullopt; }

    auto const jsonObj = assets[assetIdx].toObject();
    downloadInfo.assetDownloadURL = jsonObj["browser_download_url"].toString();
    downloadInfo.assetHash = extractHash(jsonObj["digest"].toString());
    downloadInfo.assetSize = jsonObj["size"].toInteger();

    return downloadInfo;
}

QStringView UpdateManager::normalizeVersionQt(QStringView version) {
    if (version.startsWith(u'v', Qt::CaseInsensitive)) { return version.mid(1); }
    return version;
}

int UpdateManager::compareVersionsQt(QAnyStringView vLocal, QAnyStringView vRemote) {
    auto const localVersion = QVersionNumber::fromString(vLocal);
    auto const remoteVersion = QVersionNumber::fromString(vRemote);

    if (localVersion.isNull() || remoteVersion.isNull()) {
        Log::err("Invalid version format, local: {}, remote: {}", vLocal.toString().toStdString(),
                 vRemote.toString().toStdString());
        return 0;
    }

    return QVersionNumber::compare(localVersion, remoteVersion);
}

QString UpdateManager::extractHash(QString const& digest) {
    if (digest.startsWith(QStringLiteral("sha256:"))) { return digest.mid(sizeof("sha256:") - 1); }
    return digest;
}

UpdateInfoSummary UpdateManager::updateInfoToSummary(UpdateInfo const& updateInfo) {
    return {.releaseName = updateInfo.releaseName,
            .assetName = updateInfo.assetName,
            .assetDownloadURL = updateInfo.assetDownloadURL,
            .assetHash = updateInfo.assetHash};
}
