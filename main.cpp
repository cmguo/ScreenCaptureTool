#include "httpposter.h"

#include <QUrl>
#include <QGuiApplication>
#include <QSharedMemory>
#include <QDateTime>
#include <QSize>
#include <QDir>
#include <QThread>
#include <QPixmap>
#include <QDebug>

intptr_t findWindow(int pid, char const * titleParts[]);
int captureImage(intptr_t hwnd, char ** out, int * nout, bool all);
int findProcessId(char const * name, bool latest);
int getProcessId();
int getParentProcessId(int pid);
intptr_t getProcessHandle(int pid);
bool waitForHandle(intptr_t handle, unsigned int msec);

QPixmap captureImage(intptr_t hwnd, bool separated);

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
    QString httpProxy;
    bool waitParent = false;

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
        } else if (arg == "httpProxy") {
            httpProxy = value;
        } else if (arg == "waitParent") {
            waitParent = QByteArray(value) == "true";
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

    QGuiApplication app(argc, argv);

    // Check arguments
    if (!postUrl.isValid()) {
        qWarning() << "ScreenCaptureTool: invalid command line argument: postUrl not valid";
        return 2;
    }
    UrlPoster * poster = UrlPoster::create(postUrl);
    if (poster == nullptr) {
        return 3;
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
    intptr_t hparent = 0;
    if (waitParent) {
        hparent = getProcessHandle(getParentProcessId(pid));
        if (hparent == 0) {
            qWarning() << "ScreenCaptureTool: can't not wait for parent";
            return 3;
        }
        qDebug() << "ScreenCaptureTool: wait for parent:" << hparent;
    }
    qDebug() << "ScreenCaptureTool: interval:" << interval;
    qDebug() << "ScreenCaptureTool: maxsize:" << maxsize;
    qDebug() << "ScreenCaptureTool: maxcount:" << maxcount;
    qDebug() << "ScreenCaptureTool: captureSeparated:" << captureSeparated;
    qDebug() << "ScreenCaptureTool: postUrl:" << postUrl.toEncoded();
    qDebug() << "ScreenCaptureTool: session:" << session;
    qDebug() << "ScreenCaptureTool: httpProxy:" << httpProxy;
    qDebug() << "ScreenCaptureTool: waitParent:" << waitParent;

    // Start capture loop
    sharedPid = pid;
    qDebug() << "ScreenCaptureTool: start";
    if (!poster->config(postUrl, httpProxy)) {
        return 3;
    }

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
            poster->postImage(pixmap, name, "jpg");
            ++count;
            if (maxcount && count >= maxcount)
                break;
        }
        if (hparent) {
            if (waitForHandle(hparent, interval * 1000))
                break;
        } else {
            QThread::sleep(interval);
        }
    }

    // Exit capture loop
    delete poster;
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

