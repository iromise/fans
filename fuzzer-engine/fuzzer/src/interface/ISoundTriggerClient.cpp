#include <fuzzer/interface/ISoundTriggerClient.h>
#include <fuzzer/types/int_type.h>
#include <fuzzer/utils/random.h>
sp<IBinder> generateISoundTriggerClient() {
  // http://androidxref.com/9.0.0_r3/xref/frameworks/av/soundtrigger/SoundTrigger.cpp#127

  IntType<int32_t> intType("", "sound_trigger_module_handle_t");
  sound_trigger_module_handle_t handle =
      (sound_trigger_module_handle_t)intType.generate();
  FUZZER_LOGD("Generate sound_trigger_module_handle_t %d.", (int)handle);

  sp<SoundTriggerCallback> callback;
  sp<SoundTrigger> trigger = SoundTrigger::attach(handle, callback);
  return IInterface::asBinder(trigger);
}