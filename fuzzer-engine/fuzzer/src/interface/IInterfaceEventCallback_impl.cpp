
#include <fuzzer/interface/IInterfaceEventCallback.h>
#include <fuzzer/types/types.h>
using namespace android::net::wifi;
// This method is messy..

class MyInterfaceEventCallback
    : public android::net::wifi::BnInterfaceEventCallback,
      public IBinder::DeathRecipient {
public:
  // TODO: may be we can do bad thing here, listening...
  android::binder::Status OnClientInterfaceReady(
      const ::android::sp<::android::net::wifi::IClientInterface>
          &network_interface) {
    android::binder::Status status;
    status.fromStatusT(0);
    return status;
  }
  android::binder::Status
  OnApInterfaceReady(const ::android::sp<::android::net::wifi::IApInterface>
                         &network_interface) {
    android::binder::Status status;
    status.fromStatusT(0);
    return status;
  }
  android::binder::Status OnClientTorndownEvent(
      const ::android::sp<::android::net::wifi::IClientInterface>
          &network_interface) {
    android::binder::Status status;
    status.fromStatusT(0);
    return status;
  }
  android::binder::Status
  OnApTorndownEvent(const ::android::sp<::android::net::wifi::IApInterface>
                        &network_interface) {
    android::binder::Status status;
    status.fromStatusT(0);
    return status;
  }
  void binderDied(const wp<IBinder> &who) {}
  virtual ~MyInterfaceEventCallback() {}
};

sp<IBinder> generateIInterfaceEventCallback() {
  sp<MyInterfaceEventCallback> callback = new MyInterfaceEventCallback();
  return IInterface::asBinder(callback);
}
