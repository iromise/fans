#include <fuzzer/interface/ICameraDeviceCallbacks.h>

#include <fuzzer/utils/random.h>

sp<IBinder> generateICameraDeviceCallbacks() {
  return NULL;
  //   // Camera manager
  //   ACameraManager *mCameraManager{nullptr};
  //   ACameraIdList *mCameraIdList{nullptr};
  //   // Camera device
  //   ACameraMetadata *mCameraMetadata{nullptr};
  //   ACameraDevice *mDevice{nullptr};
  //   // Capture session
  //   ACaptureSessionOutputContainer *mOutputs{nullptr};
  //   ACaptureSessionOutput *mImgReaderOutput{nullptr};
  //   ACameraCaptureSession *mSession{nullptr};
  //   // Capture request
  //   ACaptureRequest *mCaptureRequest{nullptr};
  //   ACameraOutputTarget *mReqImgReaderOutput{nullptr};

  //   bool mIsCameraReady{false};
  //   const char *mCameraId{nullptr};

  //   mCameraManager = ACameraManager_create();
  //   if (mCameraManager == nullptr) {
  //     ALOGE("Failed to create ACameraManager.");
  //     return -1;
  //   }

  //   int ret = ACameraManager_getCameraIdList(mCameraManager, &mCameraIdList);
  //   if (ret != AMEDIA_OK) {
  //     ALOGE("Failed to get cameraIdList: ret=%d", ret);
  //     return ret;
  //   }
  //   if (mCameraIdList->numCameras < 1) {
  //     ALOGW("Device has no NDK compatible camera.");
  //     return 0;
  //   }
  //   ALOGI("Found %d camera(s).", mCameraIdList->numCameras);

  //   // We always use the first camera.
  //   mCameraId =
  //       mCameraIdList->cameraIds[randomUInt64(0, mCameraIdList->numCameras -
  //       1)];
  //   if (mCameraId == nullptr) {
  //     ALOGE("Failed to get cameraId.");
  //     return -1;
  //   }

  //   ret = ACameraManager_openCamera(mCameraManager, mCameraId, &mDeviceCb,
  //                                   &mDevice);
  //   if (ret != AMEDIA_OK || mDevice == nullptr) {
  //     ALOGE("Failed to open camera, ret=%d, mDevice=%p.", ret, mDevice);
  //     return -1;
  //   }

  //   sp<hardware::camera2::ICameraDeviceCallbacks> callback =
  //       mDevice->getServiceCallback();
  //   return IInterface::asBinder(callback);
}