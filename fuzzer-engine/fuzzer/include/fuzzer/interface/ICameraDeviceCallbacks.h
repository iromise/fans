#ifndef ICAMERA_DEVICE_CALLINGBACK_H
#define ICAMERA_DEVICE_CALLINGBACK_H

#include <android/hardware/camera2/BnCameraDeviceCallbacks.h>
#include <android/hardware/camera2/ICameraDeviceUser.h>

// #include <camera/NdkCaptureRequest.h>
// #include <camera/CaptureResult.h>
// #include <camera/NdkCameraCaptureSession.h>
// #include <camera/NdkCameraDevice.h>
// #include <camera/NdkCameraError.h>
// #include <camera/NdkCameraManager.h>
// #include <camera/camera2/CaptureRequest.h>
// #include <camera/camera2/OutputConfiguration.h>
// #include <media/NdkImage.h>
// #include <media/NdkImageReader.h>
// #include <media/stagefright/foundation/AHandler.h>
// #include <media/stagefright/foundation/ALooper.h>
// #include <media/stagefright/foundation/AMessage.h>

// #include <camera/NdkCameraCaptureSession.h>
// #include <camera/NdkCameraManager.h>

using namespace android;
extern sp<IBinder> generateICameraDeviceCallbacks();
#endif // ICAMERA_DEVICE_CALLINGBACK_H