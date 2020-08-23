#ifndef ICAMERA_RECORDING_PROXY_LISTENER_H
#define ICAMERA_RECORDING_PROXY_LISTENER_H

#include <camera/CameraParameters.h>
#include <camera/ICameraRecordingProxy.h>
#include <camera/ICameraRecordingProxyListener.h>
#include <camera/android/hardware/ICamera.h>
#include <deque>
#include <gui/BufferItemConsumer.h>
#include <media/MediaSource.h>
#include <media/hardware/MetadataBufferType.h>
#include <media/stagefright/CameraSource.h>
#include <media/stagefright/MediaBuffer.h>
#include <utils/List.h>
#include <utils/RefBase.h>
#include <utils/String16.h>
using namespace android;

class MyCameraSource : public CameraSource {
public:
  static MyCameraSource *Create(const String16 &clientName);
  void mydataCallbackTimestamp(nsecs_t timestamp, int32_t msgType,
                               const sp<IMemory> &dataPtr);
  void myrecordingFrameHandleCallbackTimestamp(nsecs_t timestamp,
                                               native_handle_t *handle);
  void myrecordingFrameHandleCallbackTimestampBatch(
      const std::vector<int64_t> &timestampsUs,
      const std::vector<native_handle_t *> &handles);

protected:
  MyCameraSource(const sp<hardware::ICamera> &camera,
                 const sp<ICameraRecordingProxy> &proxy, int32_t cameraId,
                 const String16 &clientName, uid_t clientUid, pid_t clientPid,
                 Size videoSize, int32_t frameRate,
                 const sp<IGraphicBufferProducer> &surface,
                 bool storeMetaDataInVideoBuffers);
};

class ProxyListener : public BnCameraRecordingProxyListener {
public:
  ProxyListener(const sp<MyCameraSource> &source);
  virtual void dataCallbackTimestamp(int64_t timestampUs, int32_t msgType,
                                     const sp<IMemory> &data);
  virtual void recordingFrameHandleCallbackTimestamp(int64_t timestampUs,
                                                     native_handle_t *handle);
  virtual void recordingFrameHandleCallbackTimestampBatch(
      const std::vector<int64_t> &timestampsUs,
      const std::vector<native_handle_t *> &handles);

private:
  sp<MyCameraSource> mSource;
};

sp<IBinder> generateICameraRecordingProxyListener();
#endif // ICAMERA_RECORDING_PROXY_LISTENER_H