#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>

class QTcpSocket;
class QNetworkReply;
class QTimer;
class QTcpServer;

class OAuthManager final : public QObject {
    Q_OBJECT

  public:
    void tryAutoLogin();
    QString accessToken();

    void handleUnlinkGMRequested();
    void handleLoginGMRequested();
    void cancelCurrentLogin();

  Q_SIGNALS:
    void gmailLinked(QString const& email);
    void gmailUnlinked();
    void loginFailed(QString const& error = QString{});
    void loginCancelled();

  private: // NOLINT(readability-redundant-access-specifiers)
    // helper
    static QString generateRandomString(int length);
    static QString sha256Base64Url(QString const& input);
    void cleanupAuthServer();
    void processTokenJson(QJsonObject const& json);
    static void openBrowser(QUrl const& url);
    //

    void exchangeAuthCodeForTokens(QString const& authCode);
    void fetchUserEmail();
    void requestNewAccessToken(QString const& refreshToken,
                               std::function<void()> const& finishedCallback);
    void saveRefreshToken(QString const& refreshToken);
    QString loadRefreshToken();
    void handleOAuthRedirect();
    static QString
        htmlResponde(QString const& title, QString const& header, QString const& message) noexcept;
    void revokeRefreshToken(QString const& refreshTokenToRevoke);

    void handleRedirect(QTcpSocket* socket);
    void handlePostFinished(QNetworkReply* reply);

    QTcpServer* m_oauthServer{};
    QString m_codeVerifier;
    QNetworkAccessManager m_networkManager;
    QString m_accessToken;
    QDateTime m_accessTokenExpiry;
    bool m_isLogin{};
    QTimer* m_currentLoginTimer{};
};
