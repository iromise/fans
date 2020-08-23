#ifndef IAUDIO_FLINGER_CLIENT_H
#define IAUDIO_FLINGER_CLIENT_H

#include <sys/types.h>

#include <media/AudioPolicy.h>
#include <media/IAudioFlingerClient.h>
#include <media/IAudioPolicyServiceClient.h>
#include <media/MicrophoneInfo.h>
#include <system/audio.h>
#include <system/audio_effect.h>
#include <system/audio_policy.h>
#include <utils/Errors.h>
#include <utils/Mutex.h>
#include <vector>

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <math.h>
#include <media/AudioResamplerPublic.h>
#include <media/AudioSystem.h>
#include <media/IAudioFlinger.h>
#include <media/IAudioPolicyService.h>
#include <utils/Log.h>

#include <media/AudioSystem.h>
#include <system/audio.h>
using namespace android;

extern sp<IBinder> generateIAudioFlingerClient();

#endif // IAUDIO_FLINGER_CLIENT_H