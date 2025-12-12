#include <QDir>
#include <QDebug>

#include "DownloadManager.hpp"

DownloadManager::DownloadManager(QObject* parent) : QObject(parent) {
    m_timeoutTimer.setInterval(30'000); // NOLINT(readability-magic-numbers)
    m_timeoutTimer.setSingleShot(true);

    QObject::connect(&m_timeoutTimer, &QTimer::timeout, this, [this] {
        if (m_currentReply != nullptr) {
            m_currentReply->disconnect(this);
            m_currentReply->abort();
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }

        if (m_outputFile.isOpen()) { m_outputFile.close(); }
        if (!m_outputFile.fileName().isEmpty()) { m_outputFile.remove(); }

        m_timeoutTimer.stop();

        emit downloadFailCauseTimeoutRequest();
        emit downloadFailed(
            tr("Download failed.\nYour internet connection is too slow.\nPlease try again later!"));
    });
}

DownloadManager::~DownloadManager() {
    if (m_currentReply != nullptr) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
    if (m_outputFile.isOpen()) { m_outputFile.close(); }
}

void DownloadManager::startDownload(const QUrl &url, const QString &outputFilePath) {
    if (m_currentReply != nullptr) {
        emit downloadFailed(tr("Another download is already in progress."));
        return;
    }

    QNetworkRequest request(url);
    m_currentReply = m_networkManager.get(request);

    m_outputFile.setFileName(outputFilePath);
    if (!m_outputFile.open(QIODevice::WriteOnly)) {
        emit downloadFailed(tr("Cannot write to file: %1").arg(outputFilePath));
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        return;
    }

    emit downloadStarted();
    m_timeoutTimer.start();

    QObject::connect(m_currentReply, &QNetworkReply::downloadProgress, this,
                     &DownloadManager::onDownloadProgress);
    QObject::connect(m_currentReply, &QNetworkReply::finished, this,
                     &DownloadManager::onDownloadFinished);
    QObject::connect(m_currentReply, &QNetworkReply::errorOccurred, this,
                     &DownloadManager::onDownloadError);
}

void DownloadManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    emit downloadProgress(bytesReceived, bytesTotal);
    m_timeoutTimer.start();
}

void DownloadManager::onDownloadFinished() {
    if (m_currentReply == nullptr) { return; }

    if (m_currentReply->error() == QNetworkReply::NoError) {
        m_outputFile.write(m_currentReply->readAll());
        m_outputFile.close();
        emit downloadFinished(m_outputFile.fileName());
    }

    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    m_timeoutTimer.stop();
}

void DownloadManager::onDownloadError(QNetworkReply::NetworkError /*unused*/) {
    emit downloadFailed(m_currentReply->errorString());

    m_outputFile.close();
    m_outputFile.remove(); // xoá file lỗi
    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    m_timeoutTimer.stop();
}
