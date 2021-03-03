#ifndef HTTPPOSTER_H
#define HTTPPOSTER_H

#include "urlposter.h"

#include <QNetworkAccessManager>

class HttpPoster : public UrlPoster
{
    Q_OBJECT
public:
    HttpPoster();

    virtual ~HttpPoster() override;

public:
    virtual bool config(QUrl url, QString proxy) override;

    virtual void postImage(QPixmap image, QString name, QByteArray format) override;

signals:
    void postImage1(QPixmap image, QString name, QByteArray format);

private:
    void postImage2(QPixmap image, QString name, QByteArray format);

private:
    QNetworkAccessManager manager_;
    QUrl url_;
};

#endif // HTTPPOSTER_H
