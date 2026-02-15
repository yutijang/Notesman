#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QTimer>
#include <QtTypes>

class DownloadManager final : public QObject {
        Q_OBJECT

    public:
        explicit DownloadManager(QObject* parent = nullptr);
        ~DownloadManager() override;

        // Bắt đầu tải từ URL, lưu ra đường dẫn local
        void startDownload(QUrl const& url, QString const& outputFilePath);

    Q_SIGNALS:
        void downloadStarted();
        void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
        void downloadFinished(QString const& filePath);
        void downloadFailed(QString const& errorString);
        void downloadFailCauseTimeoutRequest();

    private: // NOLINT(readability-redundant-access-specifiers)
        void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
        void onDownloadFinished();
        void onDownloadError(QNetworkReply::NetworkError code);

        QNetworkAccessManager m_networkManager;
        QNetworkReply* m_currentReply{};
        QFile m_outputFile;
        QTimer m_timeoutTimer;
};
