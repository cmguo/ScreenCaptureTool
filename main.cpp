#include "widget.h"

#include <QUrl>
#include <QGuiApplication>
#include <QSharedMemory>
#include <QBuffer>
#include <QNetworkAccessManager>
#include <QHttpMultiPart>
#include <QNetworkReply>
#include <QSize>
#include <QDir>
#include <QThread>

intptr_t findWindow(int pid, char const * titleParts[]);
int captureImage(intptr_t hwnd, char ** out, int * nout, bool all);
int findProcessId(char const * name, bool latest);
int getProcessId();
int getParentProcessId(int pid);

QPixmap captureImage(intptr_t hwnd, bool separated);
void postImage(QUrl url, QString name, QIODevice * data);

int main(int argc, char *argv[])
{
    char const * captureProcess = nullptr;
    int capturePid = 0;
    char const * captureWindow = nullptr;
    intptr_t captureHwnd = 0;
    bool captureSeparated = false;
    quint32 interval = 5;
    quint32 maxcount = 0;
    QSize maxsize(0, 0);
    QUrl postUrl;
    QString session;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        char const * value = strchr(argv[i], '=');
        if (value == nullptr) {
            qWarning() << "ScreenCaptureTool: invalid argument" << argv[i];
            continue;
        }
        QByteArray arg(argv[i], value - argv[i]);
        ++value;
        if (arg == "captureProcess" || arg == "process") {
            captureProcess = value;
        } else if (arg == "capturePid" || arg == "pid") {
            capturePid = QByteArray(value).toInt();
        } else if (arg == "captureWindow" || arg == "window") {
            captureWindow = value;
        } else if (arg == "captureHwnd" || arg == "hwnd") {
            captureHwnd = QByteArray(value).toInt();
        } else if (arg == "captureSeparated" || arg == "separated") {
            captureHwnd = QByteArray(value) == "true";
        } else if (arg == "interval") {
            interval = QByteArray(value).toULong();
        } else if (arg == "maxcount") {
            maxcount = QByteArray(value).toULong();
        } else if (arg == "maxsize") {
            auto token = QByteArray(value).split('x');
            if (token.size() == 2) {
                maxsize.setWidth(token[0].toInt());
                maxsize.setHeight(token[1].toInt());
            }
        } else if (arg == "postUrl") {
            postUrl = QUrl(value);
        } else if (arg == "captureSession" || arg == "session") {
            session = value;
        } else {
            qWarning() << "ScreenCaptureTool: unknown argument" << arg;
        }
    }

    // Handle multiple instances
    int pid = getProcessId();
    QSharedMemory shared("ScreenCaptureTool-" + session);
    if (!shared.attach() && !shared.create(16)) {
        qDebug() << "ScreenCaptureTool: another capture is running, exit";
        // another attached
        return 1;
    }
    // tell another to exit
    volatile int & sharedPid = *reinterpret_cast<int*>(shared.data());
    sharedPid = 0;

    // Check arguments
    if (!postUrl.isValid()) {
        qWarning() << "ScreenCaptureTool: invalid command line argument: postUrl not valid";
        return 2;
    } else if (postUrl.isLocalFile()) {
        if (!QDir::current().mkpath(postUrl.toLocalFile())) {
            qWarning() << "ScreenCaptureTool: can't write to directory:" << postUrl.toLocalFile();
            return 3;
        }
    }
    if (interval == 0) {
        qWarning() << "ScreenCaptureTool: invalid command line argument: interval not valid";
        return 2;
    }
    if (captureHwnd == 0) {
        if (captureWindow == nullptr) {
            qWarning() << "ScreenCaptureTool: invalid command line argument: not supplying captureWindow";
            return 2;
        }
        if (capturePid == -1) {
            capturePid = getParentProcessId(getProcessId());
            if (capturePid == 0) {
                qWarning() << "ScreenCaptureTool: not found parent process";
                return 3;
            }
            qDebug() << "ScreenCaptureTool: found parent process " << pid;
        }
        if (capturePid == 0) {
            if (captureProcess) {
                capturePid = findProcessId(captureProcess, false);
                if (capturePid == 0) {
                    qWarning() << "ScreenCaptureTool: not found capture process" << captureProcess;
                    return 3;
                }
                qDebug() << "ScreenCaptureTool: found capture process " << pid;
            }
        }
        char const * titleParts[] = {captureWindow, nullptr};
        captureHwnd = findWindow(capturePid, titleParts);
        if (captureHwnd == 0) {
            qWarning() << "ScreenCaptureTool: not found capture window" << captureWindow;
            return 3;
        }
        qDebug() << "ScreenCaptureTool: found capture window:" << captureHwnd;
    }
    qDebug() << "ScreenCaptureTool: interval:" << interval;
    qDebug() << "ScreenCaptureTool: maxsize:" << maxsize;
    qDebug() << "ScreenCaptureTool: maxcount:" << maxcount;
    qDebug() << "ScreenCaptureTool: captureSeparated:" << captureSeparated;
    qDebug() << "ScreenCaptureTool: postUrl:" << postUrl.toEncoded();
    qDebug() << "ScreenCaptureTool: session:" << session;

    // Start capture loop
    sharedPid = pid;
    qDebug() << "ScreenCaptureTool: start";
    QGuiApplication app(argc, argv);
    quint32 count = 0;
    while (sharedPid == pid) {
        QString name;
        QPixmap pixmap = captureImage(captureHwnd, captureSeparated);
        if (!pixmap.isNull()) {
            if ((maxsize.width() && pixmap.width() > maxsize.width())
                    || (maxsize.height() && pixmap.height() > maxsize.height())) {
                pixmap = pixmap.scaled(maxsize.width() ? maxsize.width() : pixmap.width(),
                                       maxsize.height() ? maxsize.height() : pixmap.height(),
                                       Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            QDateTime time = QDateTime::currentDateTime();
            QString name = time.toString(Qt::DateFormat::ISODate).replace(":", "_") + ".jpg";
            qDebug() << "ScreenCaptureTool: capture a image:" << name;
            if (postUrl.isLocalFile()) {
                name = postUrl.toLocalFile()
                        + QDir::separator() + name;
                if (!pixmap.save(name, "jpg")) {
                    qWarning() << "ScreenCaptureTool: save image failed";
                }
            } else {
                QBuffer * buffer = new QBuffer;
                buffer->open(QBuffer::ReadWrite);
                pixmap.save(buffer, "jpg");
                postImage(postUrl, name, buffer);
            }
            ++count;
            if (maxcount && count >= maxcount)
                break;
        }
        QThread::sleep(interval);
    }

    // Exit capture loop
    sharedPid = 0;
    qDebug() << "ScreenCaptureTool: exit, capture count:" << count;

    return 0;
}

QPixmap captureImage(intptr_t hwnd, bool separated)
{
    char * data = nullptr;
    int size = 0;
    captureImage(hwnd, &data, &size, !separated);
    QPixmap pixmap;
    if (data && size > 0) {
        pixmap.loadFromData(reinterpret_cast<uchar*>(data), static_cast<uint>(size));
        delete[] data;
    }
    return pixmap;
}

void postImage(QUrl url, QString name, QIODevice * data)
{
    static QNetworkAccessManager manager;
    QNetworkRequest request(url);
    QHttpMultiPart * multiPart = new QHttpMultiPart();
    QHttpPart part;
    part.setRawHeader("Content-Type", "image/");
    part.setRawHeader("Content-Disposition", name.toUtf8());
    multiPart->append(part);
    data->setParent(multiPart);
    QNetworkReply * reply = manager.post(request, multiPart);
    void (QNetworkReply::* error)(QNetworkReply::NetworkError) = &QNetworkReply::error;
    QObject::connect(reply, error, [reply] (QNetworkReply::NetworkError e) {
        qWarning() << "ScreenCaptureTool: post image failed" << e << reply->errorString();
    });
    QObject::connect(reply, &QNetworkReply::finished, [reply, multiPart] {
        reply->deleteLater();
        multiPart->deleteLater();
    });
}
