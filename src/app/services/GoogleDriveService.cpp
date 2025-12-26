#include <cstdint>
#include <stdexcept>
#include <functional>
#include <qnetworkreply.h>
#include <qassert.h>
#include <qbuffer.h>
#include <qcontainerfwd.h>
#include <qcryptographichash.h>
#include <qdatetime.h>
#include <qdir.h>
#include <qhttpmultipart.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qlocale.h>
#include <qnamespace.h>
#include <qnetworkrequest.h>
#include <qobject.h>
#include <qstringview.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qurl.h>
#include <qurlquery.h>

#include "GoogleDriveService.hpp"
#include "OAuthManager.hpp"
#include "UiConstants.hpp"
#include "database_maintenance.hpp"
#include "CorePaths.hpp"
#include "Logger.hpp"
#include "helper.hpp"
#include "SettingsManager.hpp"

GoogleDriveService::GoogleDriveService(OAuthManager* oauth, QObject* parent)
    : QObject(parent), m_oauth(oauth) {
    Q_ASSERT(m_oauth != nullptr);
}

// --- BEGIN Upload/download database ---

void GoogleDriveService::uploadDatabase(const std::function<void(bool)> &done) {
    const QString filePath = CorePaths::databaseFile();
    auto* file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        Log::warn("Cannot open data.db");
        if (done) { done(false); }
        delete file;
        return;
    }

    auto* multi = new QHttpMultiPart(QHttpMultiPart::RelatedType);

    // metadata part
    QHttpPart metaPart;
    metaPart.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=UTF-8");
    metaPart.setBody(
        R"({"name":"data.db","mimeType":"application/x-sqlite3","parents":["appDataFolder"]})");
    multi->append(metaPart);

    // file part (streamed)
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-sqlite3");
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
        if (!ok) { Log::warn("Upload failed: {}", reply->errorString().toStdString()); }
        reply->deleteLater();
        if (done) { done(ok); }
    });
}

void GoogleDriveService::findAndGatherDatabaseFileInfo(
    const std::function<void(DriveFileInfo)> &done) {
    QUrl url("https://www.googleapis.com/drive/v3/files");
    QUrlQuery q;
    q.addQueryItem("q", "name='data.db' and trashed=false");
    q.addQueryItem("spaces", "appDataFolder");
    q.addQueryItem("fields", "files(id,name,size,md5Checksum,version,createdTime,modifiedTime)");

    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_oauth->accessToken().toUtf8());
    req.setRawHeader("Accept", "application/json");

    auto* reply = m_networkManager.get(req);

    QObject::connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        DriveFileInfo info;

        if (reply->error() == QNetworkReply::NoError) {
            const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
            const auto files = obj["files"].toArray();
            if (!files.isEmpty()) {
                const auto fileObj = files.first().toObject();
                info = parseFileInfo(fileObj);
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
        Log::warn("Cannot open data.db for update");
        if (done) { done(false); }
        delete file;
        return;
    }

    const QUrl url("https://www.googleapis.com/upload/drive/v3/files/" + fileId +
                   "?uploadType=media");

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_oauth->accessToken().toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-sqlite3");

    auto* reply = m_networkManager.sendCustomRequest(req, "PATCH", file);
    file->setParent(reply);

    QObject::connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        const bool ok = (reply->error() == QNetworkReply::NoError);
        if (!ok) { Log::warn("Update failed: {}", reply->errorString().toStdString()); }
        reply->deleteLater();
        if (done) { done(ok); }
    });
}

void GoogleDriveService::downloadDatabase(const QString &fileId,
                                          const std::function<void(bool)> &done) {
    const QUrl url("https://www.googleapis.com/drive/v3/files/" + fileId + "?alt=media");

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_oauth->accessToken().toUtf8());
    req.setRawHeader("Accept", "application/x-sqlite3, application/octet-stream");

    auto* reply = m_networkManager.get(req);
    const QString filePath = CorePaths::databaseFile();
    auto* file = new QFile(filePath);

    if (!file->open(QIODevice::WriteOnly)) {
        Log::warn("Download: Cannot open local file for writing");
        reply->abort();
        reply->deleteLater();
        delete file;
        if (done) { done(false); }
        return;
    }

    QObject::connect(reply, &QNetworkReply::readyRead,
                     [reply, file]() { file->write(reply->readAll()); });

    QObject::connect(reply, &QNetworkReply::finished, this, [reply, file, done]() {
        QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();

        if (reply->error() != QNetworkReply::NoError ||
            (!contentType.contains("application/x-sqlite3") &&
             !contentType.contains("application/octet-stream"))) {
            Log::warn("Download failed or invalid MIME type: {}", contentType.toStdString());
            file->close();
            file->remove();
        } else {
            file->close();
        }

        const bool ok = (reply->error() == QNetworkReply::NoError);
        file->deleteLater();
        reply->deleteLater();
        if (done) { done(ok); }
    });
}

void GoogleDriveService::uploadDbAuto() {
    emit closeConnectDBRequest(true);
    emit onUploadDBBtnRequest(true);
}

void GoogleDriveService::onConnectClosedForUpload(bool isUpload) {
    if (!isUpload) { return; }

    findAndGatherDatabaseFileInfo([this](const DriveFileInfo &info) {
        const auto localDBMD5Checksum = calculateFileMD5(CorePaths::databaseFile());

        if (info.isExists) {
            if (localDBMD5Checksum == info.md5Checksum) {
                emit onUploadDBBtnRequest(
                    false, tr("Database is already in sync, upload/update not necessary."),
                    UiConst::SettingsTabNotiLevel::good);
                emit reconnectDBRequest();
                return;
            }

            const qint64 localSavedVersion =
                SettingsManager::instance().get("db_version").toLongLong();

            if (info.version > localSavedVersion && localSavedVersion != 0) {
                // Chờ xử lý
                Log::warn("Caution: Cloud version is newer than last known local version.");
            }
        }

        try {
            const QString filePath = CorePaths::databaseFile();
            DatabaseMaintenance::compact(filePath.toStdString());
        } catch (const std::runtime_error &ex) { Log::err("Compact error: {}", ex.what()); }

        auto finish = [this, info](bool success) {
            if (success) {
                findAndGatherDatabaseFileInfo([this](const DriveFileInfo &newInfo) {
                    SettingsManager::instance().set("database/db_version", newInfo.version);
                    emit onUploadDBBtnRequest(false, tr("Compacted and uploaded new file!"),
                                              UiConst::SettingsTabNotiLevel::good);
                    emit reconnectDBRequest();
                });
            } else {
                emit onUploadDBBtnRequest(
                    false, tr("Failed to save to Drive (permission, storage, or network)"),
                    UiConst::SettingsTabNotiLevel::caution);
                emit reconnectDBRequest();
            }
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

    findAndGatherDatabaseFileInfo([this](const DriveFileInfo &info) {
        if (!info.isExists) {
            emit onDownloadDBBtnRequest(false, tr("No database found or access denied"),
                                        UiConst::SettingsTabNotiLevel::caution);
            emit reconnectDBRequest();
            return;
        }

        const auto localDBMD5Checksum = calculateFileMD5(CorePaths::databaseFile());

        if (localDBMD5Checksum == info.md5Checksum) {
            emit onDownloadDBBtnRequest(false,
                                        tr("Database is already in sync, download not necessary."),
                                        UiConst::SettingsTabNotiLevel::good);
            emit reconnectDBRequest();
            return;
        }

        downloadDatabase(info.id, [this, info](bool ok) {
            if (ok) {
                SettingsManager::instance().set("database/db_version", info.version);

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

            emit reconnectDBRequest();
        });
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
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        const bool ok = (reply->error() == QNetworkReply::NoError || statusCode == 404);

        if (!ok) {
            Log::warn("Delete file failed: {} (Status: {})", reply->errorString().toStdString(),
                      statusCode);
        }
        reply->deleteLater();
        if (done) { done(ok); }
    });
}

QString GoogleDriveService::formatDateTimeSmart(const QDateTime &dt) {
    QDateTime local = dt.toLocalTime();

    return QLocale::system().toString(local, QLocale::ShortFormat);
}

void GoogleDriveService::getDBInfo() {
    findAndGatherDatabaseFileInfo([this](const DriveFileInfo &info) {
        QStringList res;

        if (info.isExists) {
            res << info.name;
            res << QString::number(info.version);
            res << QString::fromStdString(
                Utils::normalizationDBFileSize(static_cast<std::uint64_t>(info.size)));
            res << formatDateTimeSmart(info.lastCreated);
            res << formatDateTimeSmart(info.lastModified);
        }

        emit returnDBInfo(res);
    });
}

QString GoogleDriveService::calculateFileMD5(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) { return {}; }

    QCryptographicHash hash(QCryptographicHash::Md5);
    while (!file.atEnd()) { hash.addData(file.read(8192)); } // NOLINT(readability-magic-numbers)
    return hash.result().toHex();
}

GoogleDriveService::DriveFileInfo GoogleDriveService::parseFileInfo(const QJsonObject &obj) {
    GoogleDriveService::DriveFileInfo info;

    info.id = obj["id"].toString();
    info.name = obj["name"].toString();
    info.size = obj["size"].toString().toLongLong();
    info.md5Checksum = obj["md5Checksum"].toString();
    info.version = obj["version"].toString().toLongLong();
    info.lastCreated = QDateTime::fromString(obj["createdTime"].toString(), Qt::ISODate);
    info.lastModified = QDateTime::fromString(obj["modifiedTime"].toString(), Qt::ISODate);
    info.isExists = !info.id.isEmpty();

    return info;
}

void GoogleDriveService::handleDeleteDatabaseFileRequest() {
    findAndGatherDatabaseFileInfo([this](const DriveFileInfo &info) {
        if (!info.isExists) {
            QString msg =
                tr("The %1 file does not exist, so there's no need to delete it.").arg("data.db");
            emit deleteDatabaseFileRespond(msg);
            return;
        }

        deleteDatabaseFile(info.id, [this, info](bool ok) {
            QString msgRespond;
            if (ok) {
                msgRespond = tr("Delete %1 successful!").arg(info.name);
            } else {
                msgRespond = tr("Delete %1 failed!").arg(info.name);
                Log::err("Error delete data.db");
            }

            emit deleteDatabaseFileRespond(msgRespond);
        });
    });
}
