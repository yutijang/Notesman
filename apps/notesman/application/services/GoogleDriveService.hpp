#pragma once

#include "UiConstants.hpp"

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <QtTypes>
#include <functional>

class OAuthManager;

class GoogleDriveService final : public QObject {
        Q_OBJECT

    public:
        explicit GoogleDriveService(OAuthManager* oauth, QObject* parent = nullptr);
        ~GoogleDriveService() override = default;

        void getDBInfo();

        void uploadDbAuto();
        void downloadDbAuto();

        void onConnectClosedForUpload(bool isUpload);
        void onConnectClosedForDownload(bool isUpload);

        void handleDeleteDatabaseFileRequest();

    Q_SIGNALS:
        void onDownloadDBBtnRequest(
            bool isDisable, QString const& message = QString{},
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::Normal);
        void onUploadDBBtnRequest(
            bool isDisable, QString const& message = QString{},
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::Normal);

        void closeConnectDBRequest(bool isUpload);
        void reconnectDBRequest();

        void returnDBInfo(QStringList const& res);

        void deleteDatabaseFileRespond(QString const& msg);

    private:
        struct DriveFileInfo {
                QString id;
                QString name;
                qint64 size{};
                QString md5Checksum;
                qint64 version{};
                QDateTime lastCreated;
                QDateTime lastModified;
                bool isExists{};
        };

        void uploadDatabase(std::function<void(bool)> const& done);
        void findAndGatherDatabaseFileInfo(std::function<void(DriveFileInfo)> const& done);
        void updateDatabase(QString const& fileId, std::function<void(bool)> const& done);
        void downloadDatabase(QString const& fileId, std::function<void(bool)> const& done);
        void deleteDatabaseFile(QString const& fileId, std::function<void(bool)> const& done);
        static QString formatDateTimeSmart(QDateTime const& dt);
        static QString calculateFileMD5(QString const& filePath);
        static DriveFileInfo parseFileInfo(QJsonObject const& obj);

        QNetworkAccessManager m_networkManager;
        OAuthManager* m_oauth;
};
