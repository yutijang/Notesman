#pragma once

#include <functional>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QNetworkAccessManager>
#include <QDateTime>
#include <QJsonObject>
#include <QUrl>

class QTcpSocket;
class QNetworkReply;
class QTimer;

class OAuthManager final : public QObject {
        Q_OBJECT

    public:
        void tryAutoLogin();
        QString accessToken();

    signals:
        void gmailLinked(const QString &email);
        void gmailUnlinked();
        void loginFailed(const QString &error = QString{});
        void loginCancelled();

    public slots:
        void handleUnlinkGMRequested();
        void handleLoginGMRequested();
        void cancelCurrentLogin();

    private slots:
        void handleRedirect(QTcpSocket* socket);
        void handlePostFinished(QNetworkReply* reply);

    private: // NOLINT(readability-redundant-access-specifiers)
        // helper
        static QString generateRandomString(int length);
        static QString sha256Base64Url(const QString &input);
        void cleanupAuthServer();
        void processTokenJson(const QJsonObject &json);
        static void openBrowser(const QUrl &url);
        //

        void exchangeAuthCodeForTokens(const QString &authCode);
        void fetchUserEmail();
        void requestNewAccessToken(const QString &refreshToken,
                                   const std::function<void()> &finishedCallback);
        void saveRefreshToken(const QString &refreshToken);
        QString loadRefreshToken();
        void handleOAuthRedirect();
        static QString htmlResponde(const QString &title, const QString &header,
                                    const QString &message) noexcept;
        void revokeRefreshToken(const QString &refreshTokenToRevoke);

        QTcpServer* m_oauthServer{};
        QString m_codeVerifier;
        QNetworkAccessManager m_networkManager;
        QString m_accessToken;
        QDateTime m_accessTokenExpiry;
        bool m_isLogin{};
        QTimer* m_currentLoginTimer{};
};
