#ifndef IEFFECT_CLIENT_H
#define IEFFECT_CLIENT_H

#include <stdint.h>
#include <sys/types.h>

#include <media/AudioSystem.h>
#include <media/IAudioFlinger.h>
#include <media/IAudioPolicyService.h>
#include <media/IEffect.h>
#include <media/IEffectClient.h>
#include <system/audio_effect.h>

#include <binder/IInterface.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>

#include <limits.h>
#include <stdint.h>
#include <sys/types.h>

#include <media/AudioEffect.h>
#include <private/media/AudioEffectShared.h>

#include <binder/IPCThreadState.h>
#include <utils/Log.h>

using namespace android;

extern sp<IBinder> generateIEffectClient();
#endif // IEFFECT_CLIENT_H