#ifndef FUZZER_CAMERA_H
#define FUZZER_CAMERA_H
#include <utils/Timers.h>

#include <android/hardware/ICameraService.h>

#include <camera/Camera.h>
#include <camera/CameraBase.h>
#include <camera/ICameraRecordingProxy.h>
#include <camera/ICameraRecordingProxyListener.h>
#include <camera/android/hardware/ICamera.h>
#include <camera/android/hardware/ICameraClient.h>
#include <future>
#include <gui/IGraphicBufferProducer.h>
#include <system/camera.h>
using namespace android;
extern sp<Camera> generateCamera();
#endif // FUZZER_CAMERA_H