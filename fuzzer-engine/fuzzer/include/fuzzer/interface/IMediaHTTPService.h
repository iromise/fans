#ifndef IMEDIA_HTTP_SERVICE_H
#define IMEDIA_HTTP_SERVICE_H

#include <binder/IServiceManager.h>
#include <media/IMediaHTTPService.h>
#include <media/IMediaPlayerService.h>

using namespace android;
extern sp<IBinder> generateIMediaHTTPService();

#endif // IMEDIA_HTTP_SERVICE_H