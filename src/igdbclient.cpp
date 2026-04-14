#include "igdbclient.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

IgdbClient::IgdbClient(QObject* parent)
    : QObject(parent)
    , nam_(new QNetworkAccessManager(this))
{}

void IgdbClient::set_credentials(const QString& client_id, const QString& client_secret)
{
    client_id_     = client_id.trimmed();
    client_secret_ = client_secret.trimmed();
    access_token_.clear();
    token_expiry_ = {};
}

bool IgdbClient::has_credentials() const
{
    return !client_id_.isEmpty() && !client_secret_.isEmpty();
}

bool IgdbClient::token_valid() const
{
    return !access_token_.isEmpty()
        && QDateTime::currentDateTimeUtc() < token_expiry_;
}

void IgdbClient::search(const QString& query)
{
    if (!has_credentials()) {
        emit error("IGDB credentials not configured. Add them in Settings.");
        return;
    }
    if (token_valid()) {
        do_search(query);
        return;
    }
    queued_search_ = query;
    if (!acquiring_token_)
        acquire_token();
}

void IgdbClient::acquire_token()
{
    acquiring_token_ = true;

    QNetworkRequest req(QUrl("https://id.twitch.tv/oauth2/token"));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "application/x-www-form-urlencoded");

    const QByteArray body =
        QStringLiteral("client_id=%1&client_secret=%2&grant_type=client_credentials")
        .arg(client_id_, client_secret_).toUtf8();

    auto* reply = nam_->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        acquiring_token_ = false;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit error("IGDB authentication failed: " + reply->errorString());
            queued_search_.clear();
            return;
        }

        const auto doc = QJsonDocument::fromJson(reply->readAll());
        access_token_  = doc["access_token"].toString();
        const int secs = doc["expires_in"].toInt(3600);
        token_expiry_  = QDateTime::currentDateTimeUtc().addSecs(secs - 60);

        if (!queued_search_.isEmpty()) {
            const QString q = std::exchange(queued_search_, {});
            do_search(q);
        }
    });
}

void IgdbClient::do_search(const QString& query)
{
    // Sanitize to prevent Apicalypse injection
    const QString safe = QString(query).replace('"', ' ').left(64);

    QNetworkRequest req(QUrl("https://api.igdb.com/v4/games"));
    req.setRawHeader("Client-ID",     client_id_.toUtf8());
    req.setRawHeader("Authorization", ("Bearer " + access_token_).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");

    const QByteArray body =
        QStringLiteral("fields name,cover.image_id;"
                        " where platforms = (167) & cover != null;"
                        " search \"%1\"; limit 6;")
        .arg(safe).toUtf8();

    auto* reply = nam_->post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::AuthenticationRequiredError)
                access_token_.clear();   // force re-auth next time
            emit error("IGDB search failed: " + reply->errorString());
            return;
        }

        const auto arr = QJsonDocument::fromJson(reply->readAll()).array();
        QList<GameResult> results;
        for (const auto& v : arr) {
            const QJsonObject o = v.toObject();
            const QString image_id = o["cover"]["image_id"].toString();
            if (image_id.isEmpty()) continue;
            results.append({o["name"].toString(), image_id});
        }
        emit results_ready(results);
    });
}

void IgdbClient::fetch_cover(const QString& image_id)
{
    const QUrl url =
        QStringLiteral("https://images.igdb.com/igdb/image/upload/t_cover_big/%1.jpg")
        .arg(image_id);

    auto* reply = nam_->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, image_id]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;
        QPixmap pm;
        if (pm.loadFromData(reply->readAll()))
            emit cover_ready(image_id, pm);
    });
}
