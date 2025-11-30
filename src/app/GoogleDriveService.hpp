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

        void uploadDbAuto();
        void downloadDbAuto();

    signals:
        void onDownloadDBBtnRequest(bool isDisable, const QString &message = QString{});
        void onUploadDBBtnRequest(bool isDisable, const QString &message = QString{});

    private:
        void uploadDatabase(const std::function<void(bool)> &done);
        void findDatabaseFile(const std::function<void(QString)> &done);
        void updateDatabase(const QString &fileId, const std::function<void(bool)> &done);
        void downloadDatabase(const QString &fileId, const std::function<void(bool)> &done);

        QNetworkAccessManager m_networkManager;
        OAuthManager* m_oauth;
};
