#ifndef IOMX_H
#define IOMX_H
#include <cutils/properties.h>
#include <utils/Log.h>

#include <binder/IServiceManager.h>
#include <media/stagefright/OMXClient.h>

#include <media/IOMX.h>

#include <media/omx/1.0/WOmx.h>

using namespace android;
extern sp<IBinder> generateIOMX();
#endif // IOMX_H