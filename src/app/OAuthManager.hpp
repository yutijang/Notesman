#pragma once

#include <functional>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QNetworkAccessManager>
#include <QDateTime>
#include <QJsonObject>
#include <QUrl>

class OAuthManager final : public QObject {
        Q_OBJECT

    public:
        void tryAutoLogin();

        QString accessToken();

    signals:
        void gmailLinked(const QString &email);
        void gmailUnlinked();

    public slots:
        void handleUnlinkGMRequested();
        void handleLoginGMRequested();

    private:
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

        QTcpServer* m_oauthServer{};
        QString m_codeVerifier;
        QNetworkAccessManager m_networkManager;
        QString m_accessToken;
        QDateTime m_accessTokenExpiry;
        bool m_isLogin{};
};
