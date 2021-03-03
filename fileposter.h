#ifndef FILEPOSTER_H
#define FILEPOSTER_H

#include "urlposter.h"

class FilePoster : public UrlPoster
{
    Q_OBJECT
public:
    explicit FilePoster();

public:
    virtual bool config(QUrl url, QString proxy) override;

    virtual void postImage(QPixmap image, QString name, QByteArray format) override;

private:
    QString dir_;
};

#endif // FILEPOSTER_H
