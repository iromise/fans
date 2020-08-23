#ifndef IAAUDIOCLIENT_H
#define IAAUDIOCLIENT_H
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>

#include <binder/IInterface.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <media/AudioSystem.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>
#include <utils/Singleton.h>

#include <aaudio/AAudio.h>

#include <binding/AAudioBinderClient.h>
using namespace android;
using namespace aaudio;
// using android::sp;
extern sp<IBinder> generateIAAudioClient();

#endif // IAAUDIOCLIENT_H