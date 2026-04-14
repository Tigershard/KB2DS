#pragma once

#include <QDateTime>
#include <QObject>
#include <QPixmap>

class QNetworkAccessManager;

class IgdbClient : public QObject {
    Q_OBJECT
public:
    struct GameResult {
        QString name;
        QString image_id;
    };

    explicit IgdbClient(QObject* parent = nullptr);

    void set_credentials(const QString& client_id, const QString& client_secret);
    bool has_credentials() const;

    // Search PS5 games on IGDB. Emits results_ready() or error().
    void search(const QString& query);

    // Fetch cover image (t_cover_big, 264×374). Emits cover_ready().
    void fetch_cover(const QString& image_id);

signals:
    void results_ready(QList<IgdbClient::GameResult> results);
    void cover_ready(QString image_id, QPixmap pixmap);
    void error(QString message);

private:
    bool  token_valid() const;
    void  acquire_token();
    void  do_search(const QString& query);

    QNetworkAccessManager* nam_;
    QString   client_id_;
    QString   client_secret_;
    QString   access_token_;
    QDateTime token_expiry_;
    bool      acquiring_token_ = false;
    QString   queued_search_;
};
