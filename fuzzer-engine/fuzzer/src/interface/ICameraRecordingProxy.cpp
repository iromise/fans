#include <fuzzer/interface/ICameraRecordingProxy.h>
sp<IBinder> generateICameraRecordingProxy() {
  sp<Camera> mCamera = generateCamera();
  if (mCamera == NULL) {
    return NULL;
  } else {
    return IInterface::asBinder(mCamera->getRecordingProxy());
  }
}