#include "urlposter.h"
#include "fileposter.h"
#include "httpposter.h"

UrlPoster *UrlPoster::create(QUrl url)
{
    if (url.isLocalFile()) {
        return new FilePoster();
    } else {
        return new HttpPoster();
    }
}

UrlPoster::UrlPoster()
{
}
