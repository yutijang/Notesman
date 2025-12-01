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

#include "GoogleDriveService.hpp"
#include "OAuthManager.hpp"

GoogleDriveService::GoogleDriveService(OAuthManager* oauth, QObject* parent)
    : QObject(parent), m_oauth(oauth) {
    Q_ASSERT(m_oauth != nullptr);
}

// --- BEGIN Upload/download database ---

void GoogleDriveService::uploadDatabase(const std::function<void(bool)> &done) {
    const QString filePath = QCoreApplication::applicationDirPath() + "/data.db";
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
    metaPart.setBody(R"({"name":"data.db","mimeType":"application/octet-stream"})");
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

void GoogleDriveService::findDatabaseFile(const std::function<void(QString)> &done) {
    QUrl url("https://www.googleapis.com/drive/v3/files");
    QUrlQuery q;
    q.addQueryItem("q", "name='data.db' and trashed=false");
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_oauth->accessToken().toUtf8());

    auto* reply = m_networkManager.get(req);

    QObject::connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        QString id;
        if (reply->error() == QNetworkReply::NoError) {
            const auto obj = QJsonDocument::fromJson(reply->readAll()).object();
            const auto files = obj["files"].toArray();
            if (!files.isEmpty()) { id = files.first().toObject().value("id").toString(); }
        } else {
            qWarning() << "Find file error:" << reply->errorString();
        }

        reply->deleteLater();
        if (done) {
            done(id); // nếu không có thì id="".
        }
    });
}

void GoogleDriveService::updateDatabase(const QString &fileId,
                                        const std::function<void(bool)> &done) {
    const QString filePath = QCoreApplication::applicationDirPath() + "/data.db";
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
        bool ok = (reply->error() == QNetworkReply::NoError);
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

        const QString filePath = QCoreApplication::applicationDirPath() + "/data.db";
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

    findDatabaseFile([this](const QString &id) {
        if (id.isEmpty()) {
            uploadDatabase([this](bool ok) {
                qDebug() << "Upload finished: " << ok;
                emit onUploadDBBtnRequest(false, tr("Uploaded new"));
                emit reconnectDBRequest();
            });
        } else {
            updateDatabase(id, [this](bool ok) {
                qDebug() << "Update:" << ok;
                emit onUploadDBBtnRequest(false, tr("Update completed"));
                emit reconnectDBRequest();
            });
        }
    });
}

void GoogleDriveService::downloadDbAuto() {
    emit closeConnectDBRequest(false);
    emit onDownloadDBBtnRequest(true);
}

void GoogleDriveService::onConnectClosedForDownload(bool isUpload) {
    if (isUpload) { return; }

    findDatabaseFile([this](const QString &id) {
        if (!id.isEmpty()) {
            downloadDatabase(id, [this](bool ok) {
                qDebug() << "Download finished: " << ok;
                emit onDownloadDBBtnRequest(false, tr("Download completed"));
                emit reconnectDBRequest();
            });
        } else {
            emit onDownloadDBBtnRequest(false, tr("No data.db on Drive"));
            emit reconnectDBRequest();
        }
    });
}

// --- END Upload/download database ---
