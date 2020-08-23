
#include <fuzzer/interface/camera.h>
#include <fuzzer/types/types.h>
#include <fuzzer/utils/thread.h>

using namespace std;

sp<Camera> mCamera = NULL;

sp<Camera> generateCamera() {
  String16 packageName(StringType::generatePackageName().c_str());
  int32_t cameraNum = Camera::getNumberOfCameras();
  int32_t mCameraIndex = (int32_t)randomUInt64(0, cameraNum - 1);
  FUZZER_LOGD("Camera num: %d, choose idx: %d.", cameraNum, mCameraIndex);
  mCamera = Camera::connect(mCameraIndex, packageName, Camera::USE_CALLING_UID,
                            Camera::USE_CALLING_PID);
  return mCamera;
}