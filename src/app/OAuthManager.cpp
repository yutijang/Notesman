#include <QNetworkReply>
#include <QRandomGenerator>
#include <QJsonObject>
#include <QSettings>
#include <QJsonDocument>
#include <QUrl>
#include <QUrlQuery>
#include <QDesktopServices>
#include <QTimeZone>
#include <QEventLoop>
#include <QTcpSocket>
#include <QDateTime>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QStringList>
#include <QProcess>
#include <QTimer>
#include <keychain.h>

#include "OAuthManager.hpp"
#include "google_oauth_config.hpp"

namespace {
    constexpr auto GOOGLE_OAUTH2_AUTH_URL =
        QLatin1StringView("https://accounts.google.com/o/oauth2/v2/auth");
    constexpr auto GOOGLE_OAUTH2_TOKEN_URL =
        QLatin1StringView("https://oauth2.googleapis.com/token");
    constexpr auto GOOGLE_OAUTH2_SCOPE_DRIVE =
        QLatin1StringView("openid email https://www.googleapis.com/auth/drive.file");
    constexpr auto CLIENT_ID = QLatin1StringView(OAuthConfig::CLIENT_ID);
    constexpr auto CLIENT_SECRET = QLatin1StringView(OAuthConfig::CLIENT_SECRET);
    constexpr auto REDIRECT_URI = QLatin1StringView("http://localhost:8080");
    constexpr auto SETTINGS_ORG = "Notesman";
    constexpr auto SETTINGS_APP = "configs";
    constexpr auto KEY_ACCESS_TOKEN = "google/access_token";
    constexpr auto KEY_TOKEN_EXPIRY = "google/access_token_expiry";
    constexpr auto KEY_REFRESH_TOKEN = "Notesman_google_refresh_token";
    constexpr int LOGIN_TIMEOUT{60000};
} // namespace

// --- BEGIN helper ---

QString OAuthManager::generateRandomString(int length) {
    constexpr auto possible =
        QLatin1StringView("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~");

    QString str;
    str.reserve(length);

    for (int i = 0; i < length; ++i) {
        qint64 index = QRandomGenerator::global()->bounded(possible.length());
        str.append(possible.at(index));
    }

    return str;
}

QString OAuthManager::sha256Base64Url(const QString &input) {
    QByteArray hash = QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha256);
    return QString{hash.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)};
}

void OAuthManager::cleanupAuthServer() {
    if (m_oauthServer != nullptr) {
        if (m_oauthServer->isListening()) { m_oauthServer->close(); }
        m_oauthServer->deleteLater();
        m_oauthServer = nullptr;
    }
}

void OAuthManager::processTokenJson(const QJsonObject &json) {
    if (json.isEmpty()) { return; }

    m_accessToken = json["access_token"].toString();
    const int expiresIn = json["expires_in"].toInt();
    m_accessTokenExpiry = QDateTime::currentDateTimeUtc().addSecs(expiresIn);

    // Xử lý refresh token nếu có (thường chỉ có ở lần exchange đầu tiên)
    const QString refresh = json["refresh_token"].toString();
    if (!refresh.isEmpty()) { saveRefreshToken(refresh); }

    // Lưu vào Settings
    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    settings.setValue(KEY_ACCESS_TOKEN, m_accessToken);
    settings.setValue(KEY_TOKEN_EXPIRY, m_accessTokenExpiry.toSecsSinceEpoch());
}

void OAuthManager::openBrowser(const QUrl &url) {
#ifdef _WIN32
    QDesktopServices::openUrl(url);
#else
    bool isWSL = !qEnvironmentVariable("WSL_DISTRO_NAME").isEmpty();

    if (isWSL) {
        QString urlStr = url.toString();
        QStringList args;
        args << "-Command" << QString("Start-Process \"%1\"").arg(urlStr);
        QProcess::startDetached("/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe",
                                args);
    } else {
        if (!QDesktopServices::openUrl(url)) {
            qWarning() << "Cannot open URL in default browser:" << url;
        }
    }
#endif
}

QString OAuthManager::htmlResponde(const QString &title, const QString &header,
                                   const QString &message) noexcept {
    return QStringLiteral(
               R"html(<!doctypehtml><meta charset=UTF-8><title>%1</title><style>body{font-family:Arial,sans-serif;text-align:center;margin-top:50px;background-color:#f0f2f5}h1{color:#1a73e8}</style><h1>%2</h1><p>%3</p>)html")
        .arg(title, header, message);
}

// --- END helper ---

void OAuthManager::handleOAuthRedirect() {
    QTcpSocket* socket = m_oauthServer->nextPendingConnection();

    QObject::connect(socket, &QTcpSocket::readyRead, [this, socket] { handleRedirect(socket); });
}

void OAuthManager::handleRedirect(QTcpSocket* socket) {
    if (!socket->canReadLine()) {
        return;                                        // Đợi gói tin tiếp theo
    }

    const QByteArray requestLine = socket->readLine(); // Chỉ đọc dòng đầu: GET /... HTTP/1.1
    const QString requestStr = QString::fromLatin1(requestLine);

    QRegularExpression rx(R"(GET\s+\/\?(.*)\s+HTTP\/1\.)");
    QRegularExpressionMatch match = rx.match(requestStr);

    if (!match.hasMatch()) {
        socket->disconnectFromHost();
        return;
    }

    QUrl url("http://localhost/?" + match.captured(1));
    QUrlQuery query(url);

    const QString authCode = query.queryItemValue("code");
    const QString error = query.queryItemValue("error");

    if (!authCode.isEmpty()) {
        exchangeAuthCodeForTokens(authCode);
    } else if (!error.isEmpty()) {
        qWarning() << "OAuth login failed, error:" << error;
        emit loginFailed("OAuth login was canceled");
    }

    // Trả về HTML
    QByteArray responseBody;
    if (!authCode.isEmpty()) {
        const QString title = tr("Authorization Successful");
        const QString header = tr("Authorization successful!");
        const QString body = tr("You can return to the application.");

        responseBody = htmlResponde(title, header, body).toUtf8();
    } else if (!error.isEmpty()) {
        const QString title = tr("Authorization Failed");
        const QString header = tr("Authorization failed!");
        const QString body = tr("You canceled login. Please try again.");

        responseBody = htmlResponde(title, header, body).toUtf8();
    }

    // Xây dựng HTTP Response
    QByteArray response;
    response.append("HTTP/1.1 200 OK\r\n");
    response.append("Content-Type: text/html; charset=utf-8\r\n");
    response.append("Connection: close\r\n"); // Báo hiệu đóng kết nối ngay sau khi gửi
    response.append("Content-Length: " + QByteArray::number(std::string_view(responseBody).size()) +
                    "\r\n");
    response.append("\r\n");                  // Dòng trống bắt buộc giữa Header và Body
    response.append(responseBody);

    socket->write(response);

    // Đợi byte được ghi vào buffer rồi mới ngắt kết nối
    socket->flush();
    socket->disconnectFromHost();
}

void OAuthManager::exchangeAuthCodeForTokens(const QString &authCode) {
    QUrl url(GOOGLE_OAUTH2_TOKEN_URL);

    QUrlQuery body;
    body.addQueryItem("client_id", CLIENT_ID);
    body.addQueryItem("client_secret", CLIENT_SECRET);
    body.addQueryItem("code", authCode);
    body.addQueryItem("code_verifier", m_codeVerifier);
    body.addQueryItem("redirect_uri", REDIRECT_URI);
    body.addQueryItem("grant_type", "authorization_code");

    const QByteArray bodyData = body.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    auto* reply = m_networkManager.post(req, bodyData);

    QObject::connect(reply, &QNetworkReply::finished,
                     [this, reply]() { handlePostFinished(reply); });
}

void OAuthManager::handlePostFinished(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Token exchange failed:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonDocument json = QJsonDocument::fromJson(data);
    if (!json.isObject()) {
        qWarning() << "Invalid token JSON";
        return;
    }

    processTokenJson(json.object());

    // → tiếp tục lấy email user
    fetchUserEmail();
}

void OAuthManager::fetchUserEmail() {
    QString token = accessToken();

    QNetworkRequest req(QUrl("https://www.googleapis.com/oauth2/v2/userinfo"));
    req.setRawHeader("Authorization", "Bearer " + token.toUtf8());

    auto* reply = m_networkManager.get(req);

    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();

        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            qWarning() << "Invalid userinfo JSON";
            return;
        }

        const auto obj = doc.object();
        const QString email = obj["email"].toString();
        if (email.isEmpty()) {
            qWarning() << "Email not present";
            return;
        }

        m_isLogin = true;

        emit gmailLinked(email);
    });
}

void OAuthManager::handleLoginGMRequested() {
    cleanupAuthServer();

    if (m_currentLoginTimer != nullptr) {
        m_currentLoginTimer->stop();
        m_currentLoginTimer->deleteLater();
        m_currentLoginTimer = nullptr;
    }

    constexpr int randLen{64};
    m_codeVerifier = generateRandomString(randLen);
    QString codeChallenge = sha256Base64Url(m_codeVerifier);

    // Khởi tạo server local
    constexpr int port{8080};
    m_oauthServer = new QTcpServer(this);
    QObject::connect(m_oauthServer, &QTcpServer::newConnection, this,
                     &OAuthManager::handleOAuthRedirect);

    if (!m_oauthServer->listen(QHostAddress::LocalHost, port)) {
        qWarning() << "Cannot start local OAuth server";
        emit loginFailed(
            tr("Port %1 is in use. Please close other apps using this port.").arg(port));
        return;
    }

    // Mở browser:
    QUrl authUrl(GOOGLE_OAUTH2_AUTH_URL);
    QUrlQuery query;
    query.addQueryItem("client_id", CLIENT_ID);
    query.addQueryItem("redirect_uri", REDIRECT_URI);
    query.addQueryItem("response_type", "code");
    query.addQueryItem("scope", GOOGLE_OAUTH2_SCOPE_DRIVE);
    query.addQueryItem("code_challenge", codeChallenge);
    query.addQueryItem("code_challenge_method", "S256");
    query.addQueryItem("access_type", "offline");
    authUrl.setQuery(query);

    // QDesktopServices::openUrl(authUrl);
    openBrowser(authUrl);

    m_currentLoginTimer = new QTimer(this);
    m_currentLoginTimer->setSingleShot(true);
    m_currentLoginTimer->start(LOGIN_TIMEOUT); // 60 giây
    QObject::connect(m_currentLoginTimer, &QTimer::timeout, this, [this]() {
        if (!m_isLogin) {
            emit loginFailed("OAuth login failed: timed out");
            cleanupAuthServer();
            m_currentLoginTimer = nullptr;
        }
    });
}

void OAuthManager::handleUnlinkGMRequested() {
    cleanupAuthServer();

    m_codeVerifier.clear();

    auto* job = new QKeychain::DeletePasswordJob("Notesman", this);
    job->setKey(KEY_REFRESH_TOKEN);

    QObject::connect(job, &QKeychain::Job::finished, [](QKeychain::Job* j) {
        if (j->error()) {
            qWarning() << "Failed to delete refresh token:" << j->errorString();
        } else {
            qDebug() << "Refresh token deleted successfully.";
        }
        j->deleteLater();
    });

    job->start();

    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    settings.remove(KEY_ACCESS_TOKEN);
    settings.remove(KEY_TOKEN_EXPIRY);

    m_isLogin = false;

    emit gmailUnlinked();
}

void OAuthManager::requestNewAccessToken(const QString &refreshToken,
                                         const std::function<void()> &finishedCallback) {
    QUrl url(GOOGLE_OAUTH2_TOKEN_URL);
    QUrlQuery body;
    body.addQueryItem("client_id", CLIENT_ID);
    body.addQueryItem("client_secret", CLIENT_SECRET);
    body.addQueryItem("refresh_token", refreshToken);
    body.addQueryItem("grant_type", "refresh_token");

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    auto* reply = m_networkManager.post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, finishedCallback]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Request new access token failed:" << reply->errorString();
            reply->deleteLater();
            return;
        }

        const auto json = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();

        if (json.isObject()) { processTokenJson(json.object()); }

        if (finishedCallback) { finishedCallback(); }
    });
}

void OAuthManager::saveRefreshToken(const QString &refreshToken) {
    auto* job = new QKeychain::WritePasswordJob("Notesman", this);
    job->setKey(KEY_REFRESH_TOKEN);
    job->setTextData(refreshToken);

    QObject::connect(job, &QKeychain::Job::finished, [](QKeychain::Job* j) {
        if (j->error()) {
            qWarning() << "Failed to save refresh token:" << j->errorString();
        } else {
            qDebug() << "Refresh token saved securely.";
        }
        j->deleteLater();
    });

    job->start();
}

QString OAuthManager::loadRefreshToken() {
    QKeychain::ReadPasswordJob job("Notesman", this);
    job.setKey(KEY_REFRESH_TOKEN);

    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, [&]() { loop.quit(); });
    job.start();
    loop.exec();

    if (job.error() != 0) {
        qWarning() << "No refresh token found:" << job.errorString();
        return {};
    }

    return job.textData();
}

QString OAuthManager::accessToken() {
    // Kiểm tra còn hạn offset 30 giây
    if (m_accessToken.isEmpty() ||
        QDateTime::currentDateTimeUtc() >=
            m_accessTokenExpiry.addSecs(-30)) { // NOLINT(readability-magic-numbers)
        const QString refresh = loadRefreshToken();
        if (!refresh.isEmpty()) {
            QEventLoop loop;
            requestNewAccessToken(refresh, [&]() { loop.quit(); });
            loop.exec();
        } else {
            qWarning() << "No refresh token available, login required";
            return {};
        }
    }

    return m_accessToken;
}

void OAuthManager::tryAutoLogin() {
    if (m_isLogin) { return; }

    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    m_accessToken = settings.value(KEY_ACCESS_TOKEN).toString();
    qint64 expirySecs = settings.value(KEY_TOKEN_EXPIRY).toLongLong();
    m_accessTokenExpiry = QDateTime::fromSecsSinceEpoch(expirySecs, QTimeZone::utc());

    QString token = accessToken();
    if (!token.isEmpty()) {
        qDebug() << "Auto login OK, can use access token now";
        fetchUserEmail();
    } else {
        qDebug() << "Need manual login";
    }
}

void OAuthManager::cancelCurrentLogin() {
    if (m_currentLoginTimer != nullptr) {
        m_currentLoginTimer->stop();
        m_currentLoginTimer->deleteLater();
        m_currentLoginTimer = nullptr;
    }
    cleanupAuthServer();
}
