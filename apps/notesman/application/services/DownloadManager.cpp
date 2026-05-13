#include "DownloadManager.hpp"

#include "Logger.hpp"

#include <QDir>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QTimer>
#include <QtGlobal>
#include <QtTypes>

namespace {

constexpr int DEFAULT_TIMEOUT_MS{10'000};

} // namespace

DownloadManager::DownloadManager(QObject* parent) : QObject(parent) {
    m_timeoutTimer.setInterval(DEFAULT_TIMEOUT_MS); // NOLINT(readability-magic-numbers)
    m_timeoutTimer.setSingleShot(true);

    QObject::connect(&m_timeoutTimer, &QTimer::timeout, this, [this] {
        abortDownload();

        Log::info("Stop download update because internet connection is too slow");

        Q_EMIT downloadTimedOut();
        Q_EMIT downloadFailed(
            tr("Download failed.\nYour internet connection is too slow.\nPlease try again later!"));
    });
}

DownloadManager::~DownloadManager() {
    cleanupDownloadSession(false);
}

void DownloadManager::startDownload(QUrl const& url, QString const& outputFilePath) {
    m_isAborted = false;

    if (m_currentReply != nullptr) {
        Log::info("Another download is already in progress.");
        Q_EMIT downloadFailed(tr("Another download is already in progress."));
        return;
    }

    QNetworkRequest request(url);
    m_currentReply = m_networkManager.get(request);

    m_outputFile.setFileName(outputFilePath);
    if (!m_outputFile.open(QIODevice::WriteOnly)) {
        Log::err("Cannot write to file: {}", outputFilePath.toStdString());
        Q_EMIT downloadFailed(tr("Cannot write to file: %1").arg(outputFilePath));

        if (m_currentReply != nullptr) {
            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }

        return;
    }

    Q_EMIT downloadStarted();
    m_timeoutTimer.start();

    QObject::connect(m_currentReply,
                     &QNetworkReply::downloadProgress,
                     this,
                     &DownloadManager::onDownloadProgress);
    QObject::connect(
        m_currentReply, &QNetworkReply::finished, this, &DownloadManager::onDownloadFinished);
    QObject::connect(
        m_currentReply, &QNetworkReply::errorOccurred, this, &DownloadManager::onDownloadError);
}

void DownloadManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    Q_EMIT downloadProgress(bytesReceived, bytesTotal);
    m_timeoutTimer.start();
}

void DownloadManager::onDownloadFinished() {
    if (m_currentReply == nullptr || m_isAborted) [[unlikely]] {
        Log::err("onDownloadFinished called with null reply or abort cause");
        return;
    }

    if (m_currentReply->error() == QNetworkReply::NoError) {
        m_outputFile.write(m_currentReply->readAll());
        m_outputFile.close();
        Q_EMIT downloadFinished(m_outputFile.fileName());
    }

    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    m_timeoutTimer.stop();
}

void DownloadManager::onDownloadError(QNetworkReply::NetworkError /*unused*/) {
    if (m_currentReply == nullptr || m_isAborted) [[unlikely]] {
        Log::err("onDownloadError called with null reply or abort cause");
        return;
    }

    Q_EMIT downloadFailed(m_currentReply->errorString());

    cleanupDownloadSession(true);
}

void DownloadManager::abortDownload() {
    m_isAborted = true;

    cleanupReply();

    cleanupDownloadSession(true);
}

void DownloadManager::cleanupReply() noexcept {
    if (m_currentReply == nullptr) {
        return;
    }

    m_currentReply->disconnect(this);
    m_currentReply->abort();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
}

void DownloadManager::cleanupOutputFile(bool removeFile) noexcept {
    if (m_outputFile.isOpen()) {
        m_outputFile.close();
    }

    if (removeFile && !m_outputFile.fileName().isEmpty()) {
        m_outputFile.remove();
    }
}

void DownloadManager::cleanupDownloadSession(bool const removeFile) noexcept {
    cleanupReply();

    m_timeoutTimer.stop();

    cleanupOutputFile(removeFile);
}