#ifndef DRM_SERVICE_LISTENER_H
#define DRM_SERVICE_LISTENER_H

#include <DrmManagerClientImpl.h>
#include <binder/IServiceManager.h>
#include <cutils/properties.h>
#include <utils/String8.h>
#include <utils/Vector.h>

using namespace android;
extern sp<IBinder> generateIDrmServiceListener();
#endif // DRM_SERVICE_LISTENER_H