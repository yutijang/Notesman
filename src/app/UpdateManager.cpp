#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QNetworkInformation>
#include <QTimer>
#include <QJsonArray>
#include <QJsonValue>
#include <QVersionNumber>

#include "UpdateManager.hpp"
#include "app_version.hpp"

namespace {
    constexpr int TIMEOUT{5000};
} // namespace

UpdateManager::UpdateManager(QObject* parent) : QObject(parent) {}

void UpdateManager::checkForUpdates(const QString &versionCheckUrl) {
    if (auto* netInfo = QNetworkInformation::instance()) {
        if (netInfo->reachability() == QNetworkInformation::Reachability::Disconnected) {
            emit updateCheckFailed(tr("No internet connection."));
            return;
        }
    }

    QUrl url(versionCheckUrl);
    QNetworkRequest request(url);
    request.setTransferTimeout(TIMEOUT);
    request.setRawHeader("User-Agent", "Notesman-Updater/1.0");

    QNetworkReply* reply = m_networkManager.get(request);

    auto* timeoutTimer = new QTimer(reply);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(TIMEOUT);
    QObject::connect(timeoutTimer, &QTimer::timeout, reply, [this, reply]() {
        if (reply->isRunning()) {
            reply->abort();
            emit updateCheckFailed(tr("Network connection too slow (timeout 5s)."));
        }
    });
    timeoutTimer->start();

    QObject::connect(reply, &QNetworkReply::finished, this,
                     [this, reply]() { onVersionReplyFinished(reply); });
}

void UpdateManager::onVersionReplyFinished(QNetworkReply* reply) {
    if (reply == nullptr) { return; }

    const auto status = reply->error();
    if (status != QNetworkReply::NoError) {
        emit updateCheckFailed(reply->errorString());
        reply->deleteLater();
        reply = nullptr;
        return;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();
    reply = nullptr;

    QJsonParseError parseError;
    const auto jsonDoc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
        emit updateCheckFailed(tr("Invalid update JSON: %1").arg(parseError.errorString()));
        return;
    }

    const auto releaseObj = jsonDoc.object();
    const QString latestVersion = releaseObj.value("tag_name").toString();
    const QString removeVer = normalizeVersionQt(latestVersion);
    const QString localVer = app::meta::VERSION;

    const int checkForUpdate = compareVersionsQt(localVer, removeVer);
    if (checkForUpdate == -1) {
        auto updateInfo = findAssetInfo(jsonDoc);
        if (updateInfo.has_value() && updateInfo->isValid()) {
            updateInfo->tagName = latestVersion;

            auto infoSummary = updateInfoToSummary(*updateInfo);
            emit updateAvailable(infoSummary); // ---> UpdateManager* AppController::updateManager()
        } else {
            emit updateCheckFailed(
                tr("Error gather info"));      // ---> UpdateManager* AppController::updateManager()
        }
    } else {
        emit noUpdateAvailable();              // ---> UpdateManager* AppController::updateManager()
    }
}

std::optional<UpdateManager::UpdateInfo> UpdateManager::findAssetInfo(const QJsonDocument &qJDoc) {
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

    const auto releaseObj = qJDoc.object();
    downloadInfo.releaseName = releaseObj.value("name").toString();
    downloadInfo.releaseNotes = releaseObj.value("body").toString();

    QJsonArray assets = qJDoc["assets"].toArray();

    int assetIdx{};
    for (; assetIdx < assets.size(); ++assetIdx) {
        const auto obj = assets[assetIdx].toObject();
        const auto assetName = obj["name"].toString();
        if (assetName.endsWith(preferredExt, Qt::CaseInsensitive)) {
            downloadInfo.assetName = assetName;
            break;
        }
    }

    // Nếu không tìm thấy asset, thông tin không toàn vẹn, trả về thông tin rỗng mặc định
    if (downloadInfo.assetName.isEmpty()) { return std::nullopt; }

    const auto jsonObj = assets[assetIdx].toObject();
    downloadInfo.assetDownloadURL = jsonObj["browser_download_url"].toString();
    downloadInfo.assetHash = extractHash(jsonObj["digest"].toString());
    downloadInfo.assetSize = jsonObj["size"].toInteger();

    return downloadInfo;
}

QString UpdateManager::normalizeVersionQt(const QString &version) {
    if (version.startsWith("v", Qt::CaseInsensitive)) { return version.mid(1); }
    return version;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
int UpdateManager::compareVersionsQt(const QString &vLocal, const QString &vRemote) {
    QVersionNumber localVersion = QVersionNumber::fromString(vLocal);
    QVersionNumber remoteVersion = QVersionNumber::fromString(vRemote);

    if (localVersion.isNull() || remoteVersion.isNull()) {
        qDebug() << "Lỗi: Định dạng phiên bản không hợp lệ.";
        return 0;
    }

    if (localVersion < remoteVersion) { return -1; }
    if (localVersion > remoteVersion) { return 1; }

    return 0;
}

QString UpdateManager::extractHash(const QString &digest) {
    if (digest.startsWith(QStringLiteral("sha256:"))) { return digest.mid(sizeof("sha256:") - 1); }
    return digest;
}

UpdateInfoSummary UpdateManager::updateInfoToSummary(const UpdateInfo &updateInfo) {
    return {.releaseName = updateInfo.releaseName,
            .assetName = updateInfo.assetName,
            .assetDownloadURL = updateInfo.assetDownloadURL,
            .assetHash = updateInfo.assetHash};
}
