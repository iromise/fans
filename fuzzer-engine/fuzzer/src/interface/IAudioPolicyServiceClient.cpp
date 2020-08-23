#include <fuzzer/interface/IAudioPolicyServiceClient.h>

// must before AudioFormat.h to include jni related structure.

#include <fuzzer/utils/java_vm.h>

#include <android_media_AudioFormat.h>

static struct {
  jmethodID postDynPolicyEventFromNative;
  jmethodID postRecordConfigEventFromNative;
} gAudioPolicyEventHandlerMethods;
static const char *const kClassPathName = "android/media/AudioSystem";

typedef void (*dynamic_policy_callback)(int event, String8 regId, int val);

typedef void (*record_config_callback)(int event,
                                       const record_client_info_t *clientInfo,
                                       const audio_config_base_t *clientConfig,
                                       const audio_config_base_t *deviceConfig,
                                       audio_patch_handle_t patchHandle);
static Mutex gLock;

class AudioPortCallback : public RefBase {
public:
  AudioPortCallback() {}
  virtual ~AudioPortCallback() {}

  virtual void onAudioPortListUpdate() = 0;
  virtual void onAudioPatchListUpdate() = 0;
  virtual void onServiceDied() = 0;
};

class AudioPolicyServiceClient : public IBinder::DeathRecipient,
                                 public BnAudioPolicyServiceClient {
public:
  AudioPolicyServiceClient() {
    gAudioPolicyEventHandlerMethods.postDynPolicyEventFromNative =
        GetStaticMethodIDOrDie(env, env->FindClass(kClassPathName),
                               "dynamicPolicyCallbackFromNative",
                               "(ILjava/lang/String;I)V");
    gAudioPolicyEventHandlerMethods.postRecordConfigEventFromNative =
        GetStaticMethodIDOrDie(env, env->FindClass(kClassPathName),
                               "recordingCallbackFromNative", "(IIII[I)V");
  }

  int addAudioPortCallback(const sp<AudioPortCallback> &callback);
  int removeAudioPortCallback(const sp<AudioPortCallback> &callback);
  bool isAudioPortCbEnabled() const {
    return (mAudioPortCallbacks.size() != 0);
  }

  // DeathRecipient
  virtual void binderDied(const wp<IBinder> &who);

  // IAudioPolicyServiceClient
  virtual void onAudioPortListUpdate();
  virtual void onAudioPatchListUpdate();
  virtual void onDynamicPolicyMixStateUpdate(String8 regId, int32_t state);
  virtual void
  onRecordingConfigurationUpdate(int event,
                                 const record_client_info_t *clientInfo,
                                 const audio_config_base_t *clientConfig,
                                 const audio_config_base_t *deviceConfig,
                                 audio_patch_handle_t patchHandle);

private:
  Mutex mLock;
  Vector<sp<AudioPortCallback>> mAudioPortCallbacks;
};

int AudioPolicyServiceClient::addAudioPortCallback(
    const sp<AudioPortCallback> &callback) {
  Mutex::Autolock _l(mLock);
  for (size_t i = 0; i < mAudioPortCallbacks.size(); i++) {
    if (mAudioPortCallbacks[i] == callback) {
      return -1;
    }
  }
  mAudioPortCallbacks.add(callback);
  return mAudioPortCallbacks.size();
}

int AudioPolicyServiceClient::removeAudioPortCallback(
    const sp<AudioPortCallback> &callback) {
  Mutex::Autolock _l(mLock);
  size_t i;
  for (i = 0; i < mAudioPortCallbacks.size(); i++) {
    if (mAudioPortCallbacks[i] == callback) {
      break;
    }
  }
  if (i == mAudioPortCallbacks.size()) {
    return -1;
  }
  mAudioPortCallbacks.removeAt(i);
  return mAudioPortCallbacks.size();
}

void AudioPolicyServiceClient::onAudioPortListUpdate() {
  Mutex::Autolock _l(mLock);
  for (size_t i = 0; i < mAudioPortCallbacks.size(); i++) {
    mAudioPortCallbacks[i]->onAudioPortListUpdate();
  }
}

void AudioPolicyServiceClient::onAudioPatchListUpdate() {
  Mutex::Autolock _l(mLock);
  for (size_t i = 0; i < mAudioPortCallbacks.size(); i++) {
    mAudioPortCallbacks[i]->onAudioPatchListUpdate();
  }
}

static void android_media_AudioSystem_dyn_policy_callback(int event,
                                                          String8 regId,
                                                          int val) {
  if (env == NULL) {
    return;
  }

  jclass clazz = env->FindClass(kClassPathName);
  const char *zechars = regId.string();
  jstring zestring = env->NewStringUTF(zechars);

  env->CallStaticVoidMethod(
      clazz, gAudioPolicyEventHandlerMethods.postDynPolicyEventFromNative,
      event, zestring, val);

  env->ReleaseStringUTFChars(zestring, zechars);
  env->DeleteLocalRef(clazz);
}

void AudioPolicyServiceClient::onDynamicPolicyMixStateUpdate(String8 regId,
                                                             int32_t state) {
  ALOGV("AudioPolicyServiceClient::onDynamicPolicyMixStateUpdate(%s, %d)",
        regId.string(), state);
  dynamic_policy_callback cb = NULL;
  {
    Mutex::Autolock _l(gLock);
    cb = android_media_AudioSystem_dyn_policy_callback;
  }

  if (cb != NULL) {
    cb(DYNAMIC_POLICY_EVENT_MIX_STATE_UPDATE, regId, state);
  }
}

static void android_media_AudioSystem_recording_callback(
    int event, const record_client_info_t *clientInfo,
    const audio_config_base_t *clientConfig,
    const audio_config_base_t *deviceConfig, audio_patch_handle_t patchHandle) {
  if (env == NULL) {
    return;
  }
  if (clientInfo == NULL || clientConfig == NULL || deviceConfig == NULL) {
    ALOGE("Unexpected null client/device info or configurations in recording "
          "callback");
    return;
  }

  // create an array for 2*3 integers to store the record configurations (client
  // + device)
  //                 plus 1 integer for the patch handle
  const int REC_PARAM_SIZE = 7;
  jintArray recParamArray = env->NewIntArray(REC_PARAM_SIZE);
  if (recParamArray == NULL) {
    ALOGE("recording callback: Couldn't allocate int array for configuration "
          "data");
    return;
  }
  jint recParamData[REC_PARAM_SIZE];
  recParamData[0] = (jint)audioFormatFromNative(clientConfig->format);
  // FIXME this doesn't support index-based masks
  recParamData[1] = (jint)inChannelMaskFromNative(clientConfig->channel_mask);
  recParamData[2] = (jint)clientConfig->sample_rate;
  recParamData[3] = (jint)audioFormatFromNative(deviceConfig->format);
  // FIXME this doesn't support index-based masks
  recParamData[4] = (jint)inChannelMaskFromNative(deviceConfig->channel_mask);
  recParamData[5] = (jint)deviceConfig->sample_rate;
  recParamData[6] = (jint)patchHandle;
  env->SetIntArrayRegion(recParamArray, 0, REC_PARAM_SIZE, recParamData);

  // callback into java
  jclass clazz = env->FindClass(kClassPathName);
  env->CallStaticVoidMethod(
      clazz, gAudioPolicyEventHandlerMethods.postRecordConfigEventFromNative,
      event, (jint)clientInfo->uid, clientInfo->session, clientInfo->source,
      recParamArray);
  env->DeleteLocalRef(clazz);

  env->DeleteLocalRef(recParamArray);
}

void AudioPolicyServiceClient::onRecordingConfigurationUpdate(
    int event, const record_client_info_t *clientInfo,
    const audio_config_base_t *clientConfig,
    const audio_config_base_t *deviceConfig, audio_patch_handle_t patchHandle) {
  record_config_callback cb = NULL;
  {
    Mutex::Autolock _l(gLock);
    cb = android_media_AudioSystem_recording_callback;
  }

  if (cb != NULL) {
    cb(event, clientInfo, clientConfig, deviceConfig, patchHandle);
  }
}

void AudioPolicyServiceClient::binderDied(const wp<IBinder> &who __unused) {
  {
    Mutex::Autolock _l(mLock);
    for (size_t i = 0; i < mAudioPortCallbacks.size(); i++) {
      mAudioPortCallbacks[i]->onServiceDied();
    }
  }
  // {
  //   Mutex::Autolock _l(gLockAPS);
  //   AudioSystem::gAudioPolicyService.clear();
  // }

  ALOGW("AudioPolicyService server died!");
}

sp<IBinder> generateIAudioPolicyServiceClient() {
  // if (AudioSystem::gAudioPolicyServiceClient == NULL) {
  //   sp<IAudioPolicyService> policyService =
  //       AudioSystem::get_audio_policy_service();
  sp<AudioPolicyServiceClient> gAudioPolicyServiceClient =
      new AudioPolicyServiceClient();
  return IInterface::asBinder(gAudioPolicyServiceClient);
}