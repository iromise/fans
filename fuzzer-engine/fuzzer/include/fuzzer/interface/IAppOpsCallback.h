#ifndef IAPP_OPS_CALL_BACK_H
#define IAPP_OPS_CALL_BACK_H

#include <android/hardware/ICamera.h>
#include <android/hardware/ICameraClient.h>

using namespace android;

extern sp<IBinder> generateIAppOpsCallback();

#endif // IAPP_OPS_CALL_BACK_H