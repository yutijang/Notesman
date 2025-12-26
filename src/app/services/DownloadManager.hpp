#pragma once

#include <qdir.h>
#include <qnetworkaccessmanager.h>
#include <qnetworkreply.h>
#include <qobject.h>
#include <qtimer.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qurl.h>

class DownloadManager final : public QObject {
        Q_OBJECT

    public:
        explicit DownloadManager(QObject* parent = nullptr);
        ~DownloadManager() override;

        // Bắt đầu tải từ URL, lưu ra đường dẫn local
        void startDownload(const QUrl &url, const QString &outputFilePath);

    signals:
        void downloadStarted();
        void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
        void downloadFinished(const QString &filePath);
        void downloadFailed(const QString &errorString);
        void downloadFailCauseTimeoutRequest();

    private slots:
        void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
        void onDownloadFinished();
        void onDownloadError(QNetworkReply::NetworkError code);

    private: // NOLINT(readability-redundant-access-specifiers)
        QNetworkAccessManager m_networkManager;
        QNetworkReply* m_currentReply{};
        QFile m_outputFile;
        QTimer m_timeoutTimer;
};
