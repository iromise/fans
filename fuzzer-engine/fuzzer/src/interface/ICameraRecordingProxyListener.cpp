#include <fuzzer/interface/ICameraRecordingProxyListener.h>
#include <fuzzer/types/string_type.h>

#include <OMX_Component.h>
#include <binder/IPCThreadState.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <cutils/properties.h>
#include <gui/Surface.h>
#include <media/hardware/HardwareAPI.h>
#include <media/stagefright/CameraSource.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/foundation/ADebug.h>
#include <utils/String8.h>

MyCameraSource *MyCameraSource::Create(const String16 &clientName) {
  Size size;
  size.width = -1;
  size.height = -1;

  sp<hardware::ICamera> camera;
  return new MyCameraSource(camera, NULL, 0, clientName,
                            Camera::USE_CALLING_UID, Camera::USE_CALLING_PID,
                            size, -1, NULL, false);
}
MyCameraSource::MyCameraSource(const sp<hardware::ICamera> &camera,
                               const sp<ICameraRecordingProxy> &proxy,
                               int32_t cameraId, const String16 &clientName,
                               uid_t clientUid, pid_t clientPid, Size videoSize,
                               int32_t frameRate,
                               const sp<IGraphicBufferProducer> &surface,
                               bool storeMetaDataInVideoBuffers)
    : CameraSource(camera, proxy, cameraId, clientName, clientUid, clientPid,
                   videoSize, frameRate, surface, storeMetaDataInVideoBuffers) {
}

void MyCameraSource::mydataCallbackTimestamp(nsecs_t timestamp, int32_t msgType,
                                             const sp<IMemory> &dataPtr) {
  dataCallbackTimestamp(timestamp / 1000, msgType, dataPtr);
}
void MyCameraSource::myrecordingFrameHandleCallbackTimestamp(
    nsecs_t timestamp, native_handle_t *handle) {
  recordingFrameHandleCallbackTimestamp(timestamp / 1000, handle);
}
void MyCameraSource::myrecordingFrameHandleCallbackTimestampBatch(
    const std::vector<int64_t> &timestampsUs,
    const std::vector<native_handle_t *> &handles) {
  int n = timestampsUs.size();
  std::vector<nsecs_t> modifiedTimestamps(n);
  for (int i = 0; i < n; i++) {
    modifiedTimestamps[i] = timestampsUs[i] / 1000;
  }
  recordingFrameHandleCallbackTimestampBatch(modifiedTimestamps, handles);
}

ProxyListener::ProxyListener(const sp<MyCameraSource> &source) {
  mSource = source;
}

void ProxyListener::dataCallbackTimestamp(nsecs_t timestamp, int32_t msgType,
                                          const sp<IMemory> &dataPtr) {
  mSource->mydataCallbackTimestamp(timestamp / 1000, msgType, dataPtr);
}

void ProxyListener::recordingFrameHandleCallbackTimestamp(
    nsecs_t timestamp, native_handle_t *handle) {
  mSource->myrecordingFrameHandleCallbackTimestamp(timestamp / 1000, handle);
}

void ProxyListener::recordingFrameHandleCallbackTimestampBatch(
    const std::vector<int64_t> &timestampsUs,
    const std::vector<native_handle_t *> &handles) {
  int n = timestampsUs.size();
  std::vector<nsecs_t> modifiedTimestamps(n);
  for (int i = 0; i < n; i++) {
    modifiedTimestamps[i] = timestampsUs[i] / 1000;
  }
  mSource->myrecordingFrameHandleCallbackTimestampBatch(modifiedTimestamps,
                                                        handles);
}

sp<IBinder> generateICameraRecordingProxyListener() {
  sp<MyCameraSource> cameraSource = MyCameraSource::Create(
      String16(StringType::generatePackageName().c_str()));
  sp<ProxyListener> listener = new ProxyListener(cameraSource);
  return IInterface::asBinder(listener);
}