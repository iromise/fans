#include <fuzzer/interface/IAAudioClient.h>

#include <fuzzer/utils/log.h>
using namespace android;

sp<IBinder> generateIAAudioClient() {
  AAudioBinderClient aaudioBinderClient;
  android::sp<AAudioBinderClient::AAudioClient> mAAudioClient;
  mAAudioClient = new AAudioBinderClient::AAudioClient(&aaudioBinderClient);
  return IInterface::asBinder(mAAudioClient);
}