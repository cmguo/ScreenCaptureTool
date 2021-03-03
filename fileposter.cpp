#include "fileposter.h"

#include <QDir>
#include <QDebug>

FilePoster::FilePoster()
{
}

bool FilePoster::config(QUrl url, QString)
{
    dir_ = url.toLocalFile();
    if (!QDir::current().mkpath(dir_)) {
        qWarning() << "ScreenCaptureTool: can't write to directory:" << dir_;
        return false;
    }
    return true;
}

void FilePoster::postImage(QPixmap image, QString name, QByteArray format)
{
    name = dir_ + QDir::separator() + name;
    if (!image.save(name, format)) {
        qWarning() << "ScreenCaptureTool: save image failed";
    }
}
