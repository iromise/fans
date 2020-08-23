#ifndef ISOUND_TRIGGER_CLIENT_H
#define ISOUND_TRIGGER_CLIENT_H
#include <binder/IServiceManager.h>

#include <soundtrigger/ISoundTrigger.h>
#include <soundtrigger/ISoundTriggerClient.h>
#include <soundtrigger/ISoundTriggerHwService.h>
#include <soundtrigger/SoundTrigger.h>
#include <soundtrigger/SoundTriggerCallback.h>
using namespace android;
extern sp<IBinder> generateISoundTriggerClient();

#endif // ISOUND_TRIGGER_CLIENT_H