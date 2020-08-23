
#include <fuzzer/interface/ICamera.h>
sp<IBinder> generateICamera() {
  sp<Camera> mCamera = generateCamera();
  if (mCamera == NULL) {
    return NULL;
  } else {
    return IInterface::asBinder(mCamera->remote());
  }
}