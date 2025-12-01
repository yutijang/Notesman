#pragma once

#include <functional>
#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

class OAuthManager;

class GoogleDriveService final : public QObject {
        Q_OBJECT

    public:
        explicit GoogleDriveService(OAuthManager* oauth, QObject* parent = nullptr);
        ~GoogleDriveService() override = default;

    public slots:
        void uploadDbAuto();
        void downloadDbAuto();

        void onConnectClosedForUpload(bool isUpload);
        void onConnectClosedForDownload(bool isUpload);

    signals:
        void onDownloadDBBtnRequest(bool isDisable, const QString &message = QString{});
        void onUploadDBBtnRequest(bool isDisable, const QString &message = QString{});

        void closeConnectDBRequest(bool isUpload);
        void reconnectDBRequest();

    private:
        void uploadDatabase(const std::function<void(bool)> &done);
        void findDatabaseFile(const std::function<void(QString)> &done);
        void updateDatabase(const QString &fileId, const std::function<void(bool)> &done);
        void downloadDatabase(const QString &fileId, const std::function<void(bool)> &done);

        QNetworkAccessManager m_networkManager;
        OAuthManager* m_oauth;
};
