#ifndef URLPOSTER_H
#define URLPOSTER_H

#include <QObject>
#include <QUrl>
#include <QPixmap>

class UrlPoster : public QObject
{
    Q_OBJECT
public:
    static UrlPoster * create(QUrl url);

    explicit UrlPoster();

public:
    virtual bool config(QUrl url, QString proxy) = 0;

    virtual void postImage(QPixmap image, QString name, QByteArray format) = 0;

protected:
    QUrl url_;
};

#endif // URLPOSTER_H
