#include <fuzzer/interface/IResourceManagerClient.h>

#include <fuzzer/types/types.h>

static const int kMaxReclaimWaitTimeInUs = 500000; // 0.5s

Vector<AString> mimeVector;
Vector<AString> componentNameVector;
void initCodecInfo() {
  const sp<IMediaCodecList> mcl = MediaCodecList::getInstance();
  int32_t size = mcl->countCodecs();
  FUZZER_LOGD("codec size: %d.", size);
  // FUZZER_LOGD("possible codec: ");
  for (int i = 0; i < size; ++i) {
    sp<MediaCodecInfo> codec = mcl->getCodecInfo(i);
    componentNameVector.push_back(codec->getCodecName());
    Vector<AString> mimes;
    codec->getSupportedMimes(&mimes);
    for (size_t i = 0; i < mimes.size(); i++) {
      mimeVector.push_back(mimes[i]);
    }
  }
  FUZZER_LOGD("Finish load Codec info.");
}

struct ResourceManagerClient : public BnResourceManagerClient {
  explicit ResourceManagerClient(MediaCodec *codec) : mMediaCodec(codec) {
    reclaimResource();
  }

  // static
  // private in MediaCodec
  status_t PostAndAwaitResponse(const sp<AMessage> &msg,
                                sp<AMessage> *response) {
    status_t err = msg->postAndAwaitResponse(response);

    if (err != OK) {
      return err;
    }

    if (!(*response)->findInt32("err", &err)) {
      err = OK;
    }

    return err;
  }

  // private in MediaCodec
  status_t reclaim(sp<MediaCodec> codec, bool force) {
    sp<AMessage> msg = new AMessage(kWhatRelease, codec.get());
    msg->setInt32("reclaimed", 1);
    msg->setInt32("force", force ? 1 : 0);
    sp<AMessage> response;
    status_t ret = PostAndAwaitResponse(msg, &response);
    if (ret == -ENOENT) {
      ALOGD("MediaCodec looper is gone, skip reclaim");
      ret = OK;
    }
    return ret;
  }

  virtual bool reclaimResource() {
    sp<MediaCodec> codec = mMediaCodec.promote();
    if (codec == NULL) {
      // codec is already gone.
      return true;
    }
    // status_t err = codec->release();
    // return err == OK;
    status_t err = reclaim(codec, false);
    if (err == WOULD_BLOCK) {
      ALOGD("Wait for the client to release codec.");
      usleep(kMaxReclaimWaitTimeInUs);
      ALOGD("Try to reclaim again.");
      err = reclaim(codec, true /* force */);
    }
    if (err != OK) {
      ALOGW("ResourceManagerClient failed to release codec with err %d", err);
    }
    return (err == OK);
  }

  virtual String8 getName() {
    String8 ret;
    sp<MediaCodec> codec = mMediaCodec.promote();
    if (codec == NULL) {
      // codec is already gone.
      return ret;
    }

    AString name;
    if (codec->getName(&name) == OK) {
      ret.setTo(name.c_str());
    }
    return ret;
  }

protected:
  virtual ~ResourceManagerClient() {}

private:
  wp<MediaCodec> mMediaCodec;

  DISALLOW_EVIL_CONSTRUCTORS(ResourceManagerClient);
};

sp<IBinder> generateIResourceManagerClient() {
  sp<ALooper> looper = new ALooper;
  looper->start();
  pid_t pid = IntType<int32_t>::generatePid();
  uid_t uid = IntType<int32_t>::generateUid();
  status_t err;
  sp<MediaCodec> codec;
  sp<IResourceManagerClient> client;
  int32_t flag = randomUInt64(0, 1);

  if (flag == 0) {
    // generate by CreateByType

    int32_t idx = randomUInt64(0, mimeVector.size() - 1);
    bool encoder = false;
    if (componentNameVector[idx].find("encoder", 0) != -1) {
      encoder = true;
    } else {
      encoder = false;
    }
    FUZZER_LOGD("mime: %s, encoder: %d", mimeVector[idx].c_str(), encoder);
    codec = MediaCodec::CreateByType(looper, mimeVector[idx], encoder, &err,
                                     pid, uid);
  } else {
    // generate by CreateByComponentName
    int32_t idx = randomUInt64(0, componentNameVector.size() - 1);
    codec = MediaCodec::CreateByComponentName(looper, componentNameVector[idx],
                                              &err, pid, uid);
  }
  client = new ResourceManagerClient(codec.get());
  sp<IBinder> binder = IInterface::asBinder(client);
  return binder;
}