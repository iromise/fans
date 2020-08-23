#include <fuzzer/interface/IAudioFlingerClient.h>

class AudioDeviceCallback : public RefBase {
public:
  AudioDeviceCallback() {}
  virtual ~AudioDeviceCallback() {}

  virtual void onAudioDeviceUpdate(audio_io_handle_t audioIo,
                                   audio_port_handle_t deviceId) = 0;
};

class AudioFlingerClient : public IBinder::DeathRecipient,
                           public BnAudioFlingerClient {
public:
  AudioFlingerClient()
      : mInBuffSize(0), mInSamplingRate(0), mInFormat(AUDIO_FORMAT_DEFAULT),
        mInChannelMask(AUDIO_CHANNEL_NONE) {}

  void clearIoCache();
  status_t getInputBufferSize(uint32_t sampleRate, audio_format_t format,
                              audio_channel_mask_t channelMask,
                              size_t *buffSize);
  sp<AudioIoDescriptor> getIoDescriptor(audio_io_handle_t ioHandle);

  // DeathRecipient
  virtual void binderDied(const wp<IBinder> &who);

  // IAudioFlingerClient

  // indicate a change in the configuration of an output or input: keeps the
  // cached values for output/input parameters up-to-date in client process
  virtual void ioConfigChanged(audio_io_config_event event,
                               const sp<AudioIoDescriptor> &ioDesc);

  status_t addAudioDeviceCallback(const wp<AudioDeviceCallback> &callback,
                                  audio_io_handle_t audioIo);
  status_t removeAudioDeviceCallback(const wp<AudioDeviceCallback> &callback,
                                     audio_io_handle_t audioIo);

  audio_port_handle_t getDeviceIdForIo(audio_io_handle_t audioIo);

private:
  Mutex mLock;
  DefaultKeyedVector<audio_io_handle_t, sp<AudioIoDescriptor>> mIoDescriptors;
  DefaultKeyedVector<audio_io_handle_t, Vector<wp<AudioDeviceCallback>>>
      mAudioDeviceCallbacks;
  // cached values for recording getInputBufferSize() queries
  size_t mInBuffSize; // zero indicates cache is invalid
  uint32_t mInSamplingRate;
  audio_format_t mInFormat;
  audio_channel_mask_t mInChannelMask;
  sp<AudioIoDescriptor> getIoDescriptor_l(audio_io_handle_t ioHandle);
};

void AudioFlingerClient::clearIoCache() {
  Mutex::Autolock _l(mLock);
  mIoDescriptors.clear();
  mInBuffSize = 0;
  mInSamplingRate = 0;
  mInFormat = AUDIO_FORMAT_DEFAULT;
  mInChannelMask = AUDIO_CHANNEL_NONE;
}

void AudioFlingerClient::binderDied(const wp<IBinder> &who __unused) {
  audio_error_callback cb = NULL;
  //   {
  //     Mutex::Autolock _l(AudioSystem::gLock);
  //     AudioSystem::gAudioFlinger.clear();
  //     cb = gAudioErrorCallback;
  //   }

  // clear output handles and stream to output map caches
  clearIoCache();

  if (cb) {
    cb(DEAD_OBJECT);
  }
  ALOGW("AudioFlinger server died!");
}

void AudioFlingerClient::ioConfigChanged(audio_io_config_event event,
                                         const sp<AudioIoDescriptor> &ioDesc) {
  ALOGV("ioConfigChanged() event %d", event);

  if (ioDesc == 0 || ioDesc->mIoHandle == AUDIO_IO_HANDLE_NONE)
    return;

  audio_port_handle_t deviceId = AUDIO_PORT_HANDLE_NONE;
  Vector<wp<AudioDeviceCallback>> callbacks;

  {
    Mutex::Autolock _l(mLock);

    switch (event) {
    case AUDIO_OUTPUT_OPENED:
    case AUDIO_OUTPUT_REGISTERED:
    case AUDIO_INPUT_OPENED:
    case AUDIO_INPUT_REGISTERED: {
      sp<AudioIoDescriptor> oldDesc = getIoDescriptor_l(ioDesc->mIoHandle);
      if (oldDesc == 0) {
        mIoDescriptors.add(ioDesc->mIoHandle, ioDesc);
      } else {
        deviceId = oldDesc->getDeviceId();
        mIoDescriptors.replaceValueFor(ioDesc->mIoHandle, ioDesc);
      }

      if (ioDesc->getDeviceId() != AUDIO_PORT_HANDLE_NONE) {
        deviceId = ioDesc->getDeviceId();
        if (event == AUDIO_OUTPUT_OPENED || event == AUDIO_INPUT_OPENED) {
          ssize_t ioIndex = mAudioDeviceCallbacks.indexOfKey(ioDesc->mIoHandle);
          if (ioIndex >= 0) {
            callbacks = mAudioDeviceCallbacks.valueAt(ioIndex);
          }
        }
      }
      ALOGV("ioConfigChanged() new %s %s %d samplingRate %u, format %#x "
            "channel mask %#x "
            "frameCount %zu deviceId %d",
            event == AUDIO_OUTPUT_OPENED || event == AUDIO_OUTPUT_REGISTERED
                ? "output"
                : "input",
            event == AUDIO_OUTPUT_OPENED || event == AUDIO_INPUT_OPENED
                ? "opened"
                : "registered",
            ioDesc->mIoHandle, ioDesc->mSamplingRate, ioDesc->mFormat,
            ioDesc->mChannelMask, ioDesc->mFrameCount, ioDesc->getDeviceId());
    } break;
    case AUDIO_OUTPUT_CLOSED:
    case AUDIO_INPUT_CLOSED: {
      if (getIoDescriptor_l(ioDesc->mIoHandle) == 0) {
        ALOGW("ioConfigChanged() closing unknown %s %d",
              event == AUDIO_OUTPUT_CLOSED ? "output" : "input",
              ioDesc->mIoHandle);
        break;
      }
      ALOGV("ioConfigChanged() %s %d closed",
            event == AUDIO_OUTPUT_CLOSED ? "output" : "input",
            ioDesc->mIoHandle);

      mIoDescriptors.removeItem(ioDesc->mIoHandle);
      mAudioDeviceCallbacks.removeItem(ioDesc->mIoHandle);
    } break;

    case AUDIO_OUTPUT_CONFIG_CHANGED:
    case AUDIO_INPUT_CONFIG_CHANGED: {
      sp<AudioIoDescriptor> oldDesc = getIoDescriptor_l(ioDesc->mIoHandle);
      if (oldDesc == 0) {
        ALOGW("ioConfigChanged() modifying unknown output! %d",
              ioDesc->mIoHandle);
        break;
      }

      deviceId = oldDesc->getDeviceId();
      mIoDescriptors.replaceValueFor(ioDesc->mIoHandle, ioDesc);

      if (deviceId != ioDesc->getDeviceId()) {
        deviceId = ioDesc->getDeviceId();
        ssize_t ioIndex = mAudioDeviceCallbacks.indexOfKey(ioDesc->mIoHandle);
        if (ioIndex >= 0) {
          callbacks = mAudioDeviceCallbacks.valueAt(ioIndex);
        }
      }
      ALOGV(
          "ioConfigChanged() new config for %s %d samplingRate %u, format %#x "
          "channel mask %#x frameCount %zu frameCountHAL %zu deviceId %d",
          event == AUDIO_OUTPUT_CONFIG_CHANGED ? "output" : "input",
          ioDesc->mIoHandle, ioDesc->mSamplingRate, ioDesc->mFormat,
          ioDesc->mChannelMask, ioDesc->mFrameCount, ioDesc->mFrameCountHAL,
          ioDesc->getDeviceId());

    } break;
    }
  }
  bool callbackRemoved = false;
  // callbacks.size() != 0 =>  ioDesc->mIoHandle and deviceId are valid
  for (size_t i = 0; i < callbacks.size();) {
    sp<AudioDeviceCallback> callback = callbacks[i].promote();
    if (callback.get() != nullptr) {
      callback->onAudioDeviceUpdate(ioDesc->mIoHandle, deviceId);
      i++;
    } else {
      callbacks.removeAt(i);
      callbackRemoved = true;
    }
  }
  // clean up callback list while we are here if some clients have disappeared
  // without unregistering their callback
  if (callbackRemoved) {
    Mutex::Autolock _l(mLock);
    mAudioDeviceCallbacks.replaceValueFor(ioDesc->mIoHandle, callbacks);
  }
}

status_t AudioFlingerClient::getInputBufferSize(
    uint32_t sampleRate, audio_format_t format,
    audio_channel_mask_t channelMask, size_t *buffSize) {
  const sp<IAudioFlinger> &af = AudioSystem::get_audio_flinger();
  if (af == 0) {
    return PERMISSION_DENIED;
  }
  Mutex::Autolock _l(mLock);
  // Do we have a stale mInBuffSize or are we requesting the input buffer size
  // for new values
  if ((mInBuffSize == 0) || (sampleRate != mInSamplingRate) ||
      (format != mInFormat) || (channelMask != mInChannelMask)) {
    size_t inBuffSize = af->getInputBufferSize(sampleRate, format, channelMask);
    if (inBuffSize == 0) {
      ALOGE("MyAudioFlingerClient::getInputBufferSize failed sampleRate %d "
            "format %#x "
            "channelMask %#x",
            sampleRate, format, channelMask);
      return BAD_VALUE;
    }
    // A benign race is possible here: we could overwrite a fresher cache entry
    // save the request params
    mInSamplingRate = sampleRate;
    mInFormat = format;
    mInChannelMask = channelMask;

    mInBuffSize = inBuffSize;
  }

  *buffSize = mInBuffSize;

  return NO_ERROR;
}

sp<AudioIoDescriptor>
AudioFlingerClient::getIoDescriptor_l(audio_io_handle_t ioHandle) {
  sp<AudioIoDescriptor> desc;
  ssize_t index = mIoDescriptors.indexOfKey(ioHandle);
  if (index >= 0) {
    desc = mIoDescriptors.valueAt(index);
  }
  return desc;
}

sp<AudioIoDescriptor>
AudioFlingerClient::getIoDescriptor(audio_io_handle_t ioHandle) {
  Mutex::Autolock _l(mLock);
  return getIoDescriptor_l(ioHandle);
}

status_t AudioFlingerClient::addAudioDeviceCallback(
    const wp<AudioDeviceCallback> &callback, audio_io_handle_t audioIo) {
  Mutex::Autolock _l(mLock);
  Vector<wp<AudioDeviceCallback>> callbacks;
  ssize_t ioIndex = mAudioDeviceCallbacks.indexOfKey(audioIo);
  if (ioIndex >= 0) {
    callbacks = mAudioDeviceCallbacks.valueAt(ioIndex);
  }

  for (size_t cbIndex = 0; cbIndex < callbacks.size(); cbIndex++) {
    if (callbacks[cbIndex].unsafe_get() == callback.unsafe_get()) {
      return INVALID_OPERATION;
    }
  }
  callbacks.add(callback);

  mAudioDeviceCallbacks.replaceValueFor(audioIo, callbacks);
  return NO_ERROR;
}

status_t AudioFlingerClient::removeAudioDeviceCallback(
    const wp<AudioDeviceCallback> &callback, audio_io_handle_t audioIo) {
  Mutex::Autolock _l(mLock);
  ssize_t ioIndex = mAudioDeviceCallbacks.indexOfKey(audioIo);
  if (ioIndex < 0) {
    return INVALID_OPERATION;
  }
  Vector<wp<AudioDeviceCallback>> callbacks =
      mAudioDeviceCallbacks.valueAt(ioIndex);

  size_t cbIndex;
  for (cbIndex = 0; cbIndex < callbacks.size(); cbIndex++) {
    if (callbacks[cbIndex].unsafe_get() == callback.unsafe_get()) {
      break;
    }
  }
  if (cbIndex == callbacks.size()) {
    return INVALID_OPERATION;
  }
  callbacks.removeAt(cbIndex);
  if (callbacks.size() != 0) {
    mAudioDeviceCallbacks.replaceValueFor(audioIo, callbacks);
  } else {
    mAudioDeviceCallbacks.removeItem(audioIo);
  }
  return NO_ERROR;
}

sp<IBinder> generateIAudioFlingerClient() {
  sp<AudioFlingerClient> gAudioFlingerClient = new AudioFlingerClient();
  return IInterface::asBinder(gAudioFlingerClient);
}