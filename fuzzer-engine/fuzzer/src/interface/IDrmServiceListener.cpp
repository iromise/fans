#include <fuzzer/interface/IDrmServiceListener.h>
#include <fuzzer/types/types.h>
#include <fuzzer/utils/random.h>
sp<IBinder> generateIDrmServiceListener() {
  int32_t pUniqueId;
  bool isNative = (bool)randomUInt64(0, 1);

  sp<DrmManagerClientImpl> client =
      DrmManagerClientImpl::create(&pUniqueId, isNative);
  return IInterface::asBinder(client);
}