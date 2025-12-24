#include <functional>
#include <QString>
#include <QNetworkReply>
#include <QHttpPart>
#include <QTcpServer>
#include <QNetworkAccessManager>
#include <QCoreApplication>
#include <QFileInfo>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLocale>

#include "GoogleDriveService.hpp"
#include "OAuthManager.hpp"
#include "database_maintenance.hpp"
#include "CorePaths.hpp"
#include "Logger.hpp"

GoogleDriveService::GoogleDriveService(OAuthManager* oauth, QObject* parent)
    : QObject(parent), m_oauth(oauth) {
    Q_ASSERT(m_oauth != nullptr);
}

// --- BEGIN Upload/download database ---

void GoogleDriveService::uploadDatabase(const std::function<void(bool)> &done) {
    const QString filePath = CorePaths::databaseFile();
    auto* file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open data.db";
        if (done) { done(false); }
        delete file;
        return;
    }

    auto* multi = new QHttpMultiPart(QHttpMultiPart::RelatedType);

    // metadata part
    QHttpPart metaPart;
    metaPart.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=UTF-8");
    metaPart.setBody(
        R"({"name":"data.db","mimeType":"application/octet-stream","parents":["appDataFolder"]})");
    multi->append(metaPart);

    // file part (streamed)
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    filePart.setBodyDevice(
        file);              // QNetworkAccessManager sẽ take ownership? we manage deletion below
    file->setParent(multi); // ensure deletion with multi
    multi->append(filePart);

    QUrl url("https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart");
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_oauth->accessToken().toUtf8());

    auto* reply = m_networkManager.post(req, multi);
    multi->setParent(reply); // will be deleted with reply

    QObject::connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        const bool ok = (reply->error() == QNetworkReply::NoError);
        if (!ok) { qWarning() << "Upload failed:" << reply->errorString(); }
        reply->deleteLater();
        if (done) { done(ok); }
    });
}

void GoogleDriveService::findAndGatherDatabaseFileInfo(
    const std::function<void(UiConst::DriveFileInfo)> &done) {
    QUrl url("https://www.googleapis.com/drive/v3/files");
    QUrlQuery q;
    q.addQueryItem("q", "name='data.db' and trashed=false");
    q.addQueryItem("spaces", "appDataFolder");
    q.addQueryItem("fields", "files(id, size, modifiedTime)");

    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_oauth->accessToken().toUtf8());
    req.setRawHeader("Accept", "application/json");

    auto* reply = m_networkManager.get(req);

    QObject::connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        UiConst::DriveFileInfo info;

        if (reply->error() == QNetworkReply::NoError) {
            const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
            const auto files = obj["files"].toArray();
            if (!files.isEmpty()) {
                const auto fileObj = files.first().toObject();
                info.isExists = true;
                info.id = fileObj["id"].toString();
                info.size = fileObj["size"].toString().toLongLong();
                info.lastModified =
                    QDateTime::fromString(fileObj["modifiedTime"].toString(), Qt::ISODate);
            }
        } else {
            QByteArray errorData = reply->readAll();
            Log::info("Find file error: {} - Server response content: {}",
                      reply->errorString().toStdString(), errorData.toStdString());
        }

        reply->deleteLater();
        if (done) { done(info); }
    });
}

void GoogleDriveService::updateDatabase(const QString &fileId,
                                        const std::function<void(bool)> &done) {
    const QString filePath = CorePaths::databaseFile();
    auto* file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        if (done) { done(false); }
        return;
    }

    const QByteArray dbBytes = file->readAll();
    file->close();

    const QUrl url("https://www.googleapis.com/upload/drive/v3/files/" + fileId +
                   "?uploadType=media");

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_oauth->accessToken().toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");

    auto* reply = m_networkManager.sendCustomRequest(req, "PATCH", dbBytes);

    QObject::connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        const bool ok = (reply->error() == QNetworkReply::NoError);
        if (!ok) { qWarning() << "Update failed:" << reply->errorString(); }
        reply->deleteLater();
        if (done) { done(ok); }
    });
}

void GoogleDriveService::downloadDatabase(const QString &fileId,
                                          const std::function<void(bool)> &done) {
    const QUrl url("https://www.googleapis.com/drive/v3/files/" + fileId + "?alt=media");

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_oauth->accessToken().toUtf8());

    auto* reply = m_networkManager.get(req);

    QObject::connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Download failed:" << reply->errorString();
            reply->deleteLater();
            if (done) { done(false); }
            return;
        }

        const QString filePath = CorePaths::databaseFile();
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            reply->deleteLater();
            if (done) { done(false); }
            return;
        }

        file.write(reply->readAll());
        file.close();
        reply->deleteLater();
        if (done) { done(true); }
    });
}

void GoogleDriveService::uploadDbAuto() {
    emit closeConnectDBRequest(true);
    emit onUploadDBBtnRequest(true);
}

void GoogleDriveService::onConnectClosedForUpload(bool isUpload) {
    if (!isUpload) { return; }

    try {
        const QString filePath = CorePaths::databaseFile();
        DatabaseMaintenance::compact(filePath.toStdString());
    } catch (const std::runtime_error &ex) { qDebug() << "Compact error: " << ex.what(); }

    findAndGatherDatabaseFileInfo([this](const UiConst::DriveFileInfo &info) {
        auto finish = [this](bool success) {
            if (success) {
                emit onUploadDBBtnRequest(false, tr("Compacted and uploaded new file"),
                                          UiConst::SettingsTabNotiLevel::good);
            } else {
                emit onUploadDBBtnRequest(
                    false, tr("Failed to save to Drive (permission, storage, or network)"),
                    UiConst::SettingsTabNotiLevel::caution);
            }
            emit reconnectDBRequest();
        };

        if (!info.isExists) {
            uploadDatabase(finish);
        } else {
            updateDatabase(info.id, finish);
        }
    });
}

void GoogleDriveService::downloadDbAuto() {
    emit closeConnectDBRequest(false);
    emit onDownloadDBBtnRequest(true);
}

void GoogleDriveService::onConnectClosedForDownload(bool isUpload) {
    if (isUpload) { return; }

    findAndGatherDatabaseFileInfo([this](const UiConst::DriveFileInfo &info) {
        if (!info.isExists) {
            emit onDownloadDBBtnRequest(false, tr("No database found or access denied"),
                                        UiConst::SettingsTabNotiLevel::caution);
        } else {
            downloadDatabase(info.id, [this, info](bool ok) {
                if (ok) {
                    emit onDownloadDBBtnRequest(
                        false,
                        tr("Database downloaded successfully, with size of data.db is: %1")
                            .arg(info.size),
                        UiConst::SettingsTabNotiLevel::good);
                } else {
                    emit onDownloadDBBtnRequest(false,
                                                tr("Failed to download database. Please try again"),
                                                UiConst::SettingsTabNotiLevel::caution);
                }
            });
        }
        emit reconnectDBRequest();
    });
}

// --- END Upload/download database ---

void GoogleDriveService::deleteDatabaseFile(const QString &fileId,
                                            const std::function<void(bool)> &done) {
    if (fileId.isEmpty()) {
        if (done) { done(false); }
        return;
    }

    QUrl url("https://www.googleapis.com/drive/v3/files/" + fileId);
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_oauth->accessToken().toUtf8());

    auto* reply = m_networkManager.deleteResource(req); // Gửi lệnh DELETE

    QObject::connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        const bool ok = (reply->error() == QNetworkReply::NoError);
        if (!ok) { qWarning() << "Delete file failed:" << reply->errorString(); }
        reply->deleteLater();
        if (done) { done(ok); }
    });
}

QString GoogleDriveService::formatDateTimeSmart(const QDateTime &dt) {
    QDateTime local = dt.toLocalTime();

    return QLocale::system().toString(local, QLocale::ShortFormat);
}

void GoogleDriveService::getDBInfo() {
    findAndGatherDatabaseFileInfo([this](const UiConst::DriveFileInfo &info) {
        QStringList res;

        if (info.isExists) {
            res << QString::fromStdString(Utils::normalizationDBFileSize(info.size));
            res << formatDateTimeSmart(info.lastModified);
        }

        emit returnDBInfo(res);
    });
}
