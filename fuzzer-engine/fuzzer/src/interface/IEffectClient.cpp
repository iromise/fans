#include <fuzzer/interface/IEffectClient.h>
#include <fuzzer/types/string_type.h>
// Implements the IEffectClient interface
class MyAudioEffect : public AudioEffect {
public:
  MyAudioEffect(String16 packageName) : AudioEffect(packageName) {}
  void mycontrolStatusChanged(bool controlGranted) {
    controlStatusChanged(controlGranted);
  }
  void myenableStatusChanged(bool enabled) { enableStatusChanged(enabled); }
  void mycommandExecuted(uint32_t cmdCode, uint32_t cmdSize, void *pCmdData,
                         uint32_t replySize, void *pReplyData) {
    commandExecuted(cmdCode, cmdSize, pCmdData, replySize, pReplyData);
  }
  void mybinderDied(const wp<IBinder> & /*who*/) {
    ALOGW("IEffect died");
    mStatus = DEAD_OBJECT;
    if (mCbf != NULL) {
      status_t status = DEAD_OBJECT;
      mCbf(EVENT_ERROR, mUserData, &status);
    }
    // as mIEffect is private, we can not clear it..
    // mIEffect.clear();
  }
};
class EffectClient : public android::BnEffectClient,
                     public android::IBinder::DeathRecipient {
public:
  EffectClient(MyAudioEffect *effect) : mEffect(effect) {}

  // IEffectClient
  virtual void controlStatusChanged(bool controlGranted) {
    sp<MyAudioEffect> effect = mEffect.promote();
    if (effect != 0) {
      effect->mycontrolStatusChanged(controlGranted);
    }
  }
  virtual void enableStatusChanged(bool enabled) {
    sp<MyAudioEffect> effect = mEffect.promote();
    if (effect != 0) {
      effect->myenableStatusChanged(enabled);
    }
  }
  virtual void commandExecuted(uint32_t cmdCode, uint32_t cmdSize,
                               void *pCmdData, uint32_t replySize,
                               void *pReplyData) {
    sp<MyAudioEffect> effect = mEffect.promote();
    if (effect != 0) {
      effect->mycommandExecuted(cmdCode, cmdSize, pCmdData, replySize,
                                pReplyData);
    }
  }

  // IBinder::DeathRecipient
  virtual void binderDied(const wp<IBinder> & /*who*/) {
    sp<MyAudioEffect> effect = mEffect.promote();
    if (effect != 0) {
      effect->mybinderDied(this);
    }
  }

private:
  wp<MyAudioEffect> mEffect;
};

sp<IBinder> generateIEffectClient() {
  String16 packageName(StringType::generatePackageName().c_str());
  sp<MyAudioEffect> audioEffect = new MyAudioEffect(packageName);
  sp<EffectClient> mIEffectClient = new EffectClient(audioEffect.get());
  return IInterface::asBinder(mIEffectClient);
}