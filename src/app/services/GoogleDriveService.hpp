#pragma once

#include <functional>
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QNetworkAccessManager>

#include "UiConstants.hpp"

class OAuthManager;

class GoogleDriveService final : public QObject {
        Q_OBJECT

    public:
        explicit GoogleDriveService(OAuthManager* oauth, QObject* parent = nullptr);
        ~GoogleDriveService() override = default;

        void getDBInfo();

    public slots: // NOLINT(readability-redundant-access-specifiers)
        void uploadDbAuto();
        void downloadDbAuto();

        void onConnectClosedForUpload(bool isUpload);
        void onConnectClosedForDownload(bool isUpload);

    signals:
        void onDownloadDBBtnRequest(
            bool isDisable, const QString &message = QString{},
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::normal);
        void onUploadDBBtnRequest(
            bool isDisable, const QString &message = QString{},
            UiConst::SettingsTabNotiLevel notiType = UiConst::SettingsTabNotiLevel::normal);

        void closeConnectDBRequest(bool isUpload);
        void reconnectDBRequest();

        void returnDBInfo(const QStringList &res);

    private:
        void uploadDatabase(const std::function<void(bool)> &done);
        void findAndGatherDatabaseFileInfo(const std::function<void(UiConst::DriveFileInfo)> &done);
        void updateDatabase(const QString &fileId, const std::function<void(bool)> &done);
        void downloadDatabase(const QString &fileId, const std::function<void(bool)> &done);
        void deleteDatabaseFile(const QString &fileId, const std::function<void(bool)> &done);
        static QString formatDateTimeSmart(const QDateTime &dt);
        static QString calculateFileMD5(const QString &filePath);
        static UiConst::DriveFileInfo parseFileInfo(const QJsonObject &obj);

        QNetworkAccessManager m_networkManager;
        OAuthManager* m_oauth;
};
