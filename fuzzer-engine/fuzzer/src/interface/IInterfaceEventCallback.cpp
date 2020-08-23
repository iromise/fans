#include <android/net/wifi/BpInterfaceEventCallback.h>
#include <android/net/wifi/IInterfaceEventCallback.h>

namespace android {

namespace net {

namespace wifi {

IMPLEMENT_META_INTERFACE(InterfaceEventCallback,
                         "android.net.wifi.IInterfaceEventCallback")

} // namespace wifi

} // namespace net

} // namespace android
#include <android/net/wifi/BpInterfaceEventCallback.h>
#include <binder/Parcel.h>

namespace android {

namespace net {

namespace wifi {

BpInterfaceEventCallback::BpInterfaceEventCallback(
    const ::android::sp<::android::IBinder> &_aidl_impl)
    : BpInterface<IInterfaceEventCallback>(_aidl_impl) {}

::android::binder::Status BpInterfaceEventCallback::OnClientInterfaceReady(
    const ::android::sp<::android::net::wifi::IClientInterface>
        &network_interface) {
  // TODO:
  android::binder::Status status;
  status.fromStatusT(0);
  return status;
}

::android::binder::Status BpInterfaceEventCallback::OnApInterfaceReady(
    const ::android::sp<::android::net::wifi::IApInterface>
        &network_interface) {
  // TODO:
  android::binder::Status status;
  status.fromStatusT(0);
  return status;
}

::android::binder::Status BpInterfaceEventCallback::OnClientTorndownEvent(
    const ::android::sp<::android::net::wifi::IClientInterface>
        &network_interface) {
  // TODO:
  android::binder::Status status;
  status.fromStatusT(0);
  return status;
}

::android::binder::Status BpInterfaceEventCallback::OnApTorndownEvent(
    const ::android::sp<::android::net::wifi::IApInterface>
        &network_interface) {
  // TODO:
  android::binder::Status status;
  status.fromStatusT(0);
  return status;
}

} // namespace wifi

} // namespace net

} // namespace android
#include <android/net/wifi/BnInterfaceEventCallback.h>
#include <binder/Parcel.h>

namespace android {

namespace net {

namespace wifi {

::android::status_t BnInterfaceEventCallback::onTransact(
    uint32_t _aidl_code, const ::android::Parcel &_aidl_data,
    ::android::Parcel *_aidl_reply, uint32_t _aidl_flags) {
  ::android::status_t _aidl_ret_status = ::android::OK;
  switch (_aidl_code) {
  case Call::ONCLIENTINTERFACEREADY: {
    // TODO, affected by IClientInterface, the followings are similar
  } break;
  case Call::ONAPINTERFACEREADY: {
    // TODO
  } break;
  case Call::ONCLIENTTORNDOWNEVENT: {
    // TODO
  } break;
  case Call::ONAPTORNDOWNEVENT: {
    // TODO
  } break;
  default: {
    _aidl_ret_status = ::android::BBinder::onTransact(_aidl_code, _aidl_data,
                                                      _aidl_reply, _aidl_flags);
  } break;
  }
  if (_aidl_ret_status == ::android::UNEXPECTED_NULL) {
    _aidl_ret_status = ::android::binder::Status::fromExceptionCode(
                           ::android::binder::Status::EX_NULL_POINTER)
                           .writeToParcel(_aidl_reply);
  }
  return _aidl_ret_status;
}

} // namespace wifi

} // namespace net

} // namespace android
