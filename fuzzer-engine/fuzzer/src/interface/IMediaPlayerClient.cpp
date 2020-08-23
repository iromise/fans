#include <fuzzer/interface/IMediaPlayerClient.h>

struct MediaPlayerClient : public BnMediaPlayerClient {
  MediaPlayerClient() : mEOS(false) {}

  virtual void notify(int msg, int ext1 __unused, int ext2 __unused,
                      const Parcel *obj __unused) {
    Mutex::Autolock autoLock(mLock);

    if (msg == MEDIA_ERROR || msg == MEDIA_PLAYBACK_COMPLETE) {
      mEOS = true;
      mCondition.signal();
    }
  }

  void waitForEOS() {
    Mutex::Autolock autoLock(mLock);
    while (!mEOS) {
      mCondition.wait(mLock);
    }
  }

protected:
  virtual ~MediaPlayerClient() {}

private:
  Mutex mLock;
  Condition mCondition;

  bool mEOS;

  DISALLOW_EVIL_CONSTRUCTORS(MediaPlayerClient);
};

sp<IBinder> generateIMediaPlayerClient() {
  sp<MediaPlayerClient> client = new MediaPlayerClient;
  return IInterface::asBinder(client);
}