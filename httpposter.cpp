#include "httpposter.h"

#include <QHttpMultiPart>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QThread>
#include <QBuffer>
#include <QMimeDatabase>

static QThread & workThread()
{
    static QThread th;
    th.setObjectName("HttpPoster");
    return th;
}

HttpPoster::HttpPoster()
{
    moveToThread(&workThread());
    manager_.moveToThread(&workThread());

    connect(this, &HttpPoster::postImage1, this, &HttpPoster::postImage2);

    workThread().start();
}

HttpPoster::~HttpPoster()
{
    workThread().quit();
    workThread().wait();
}

bool HttpPoster::config(QUrl url, QString proxy)
{
    url_ = url;
    if (!proxy.isEmpty()) {
        quint16 port = 80;
        int n = proxy.lastIndexOf(':');
        if (n > 0) {
            port = proxy.mid(n + 1).toUShort();
            proxy = proxy.mid(0, n);
        }
        manager_.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, proxy, port));
    }
    return true;
}

void HttpPoster::postImage(QPixmap image, QString name, QByteArray format)
{
    postImage1(image, name, format);
}

void HttpPoster::postImage2(QPixmap image, QString name, QByteArray format)
{
    qDebug() << "ScreenCaptureTool: post image" << name;
    QNetworkRequest request(url_);
    QHttpMultiPart * multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart part;
    QString type = QMimeDatabase().mimeTypeForFileNameAndData(name, "").name();
    part.setRawHeader("Content-Type", type.toUtf8());
    part.setRawHeader("Content-Disposition",  QByteArray("form-data; name=\"file\"; filename=") + name.toUtf8());
    QBuffer * buffer = new QBuffer(multiPart);
    buffer->open(QBuffer::ReadWrite);
    image.save(buffer, format);
    buffer->seek(0);
    part.setBodyDevice(buffer);
    multiPart->append(part);
    QNetworkReply * reply = manager_.post(request, multiPart);
    void (QNetworkReply::* error)(QNetworkReply::NetworkError) = &QNetworkReply::error;
    QObject::connect(reply, error, this, [reply] (QNetworkReply::NetworkError e) {
        qWarning() << "ScreenCaptureTool: post image failed" << e << reply->errorString();
    });
    QObject::connect(reply, &QNetworkReply::finished, this, [reply, multiPart] {
        reply->deleteLater();
        multiPart->deleteLater();
        qDebug() << "ScreenCaptureTool: server respone" << reply->readAll();
    });
}
