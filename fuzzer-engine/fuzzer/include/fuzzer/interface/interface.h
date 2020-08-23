#ifndef FUZZER_INTERFACE
#define FUZZER_INTERFACE
#include <fuzzer/interface/IStreamSource.h>

#include <fuzzer/interface/IAAudioClient.h>
#include <fuzzer/interface/IAudioFlingerClient.h>
#include <fuzzer/interface/IAudioPolicyServiceClient.h>
#include <fuzzer/interface/ICamera.h>
#include <fuzzer/interface/ICameraRecordingProxy.h>
#include <fuzzer/interface/ICameraRecordingProxyListener.h>
#include <fuzzer/interface/IDisplay.h>
#include <fuzzer/interface/IDrmClient.h>
#include <fuzzer/interface/IDrmServiceListener.h>
#include <fuzzer/interface/IEffectClient.h>
#include <fuzzer/interface/IGraphicBufferProducer.h>
#include <fuzzer/interface/IMediaHTTPService.h>
#include <fuzzer/interface/IMediaPlayerClient.h>
#include <fuzzer/interface/IMediaRecorderClient.h>
#include <fuzzer/interface/IResourceManagerClient.h>
#include <fuzzer/interface/ISoundTriggerClient.h>

#include <fuzzer/interface/camera.h>

#include <fuzzer/interface/ICameraDeviceCallbacks.h>

#include <fuzzer/interface/IAppOpsCallback.h>

#include <fuzzer/interface/IGraphicBufferConsumer.h>

#include <fuzzer/interface/IPlayer.h>

#include <fuzzer/interface/IOMX.h>

#include <fuzzer/interface/IMediaLogService.h>

#include <fuzzer/interface/IInterfaceEventCallback.h>

#include <fuzzer/interface/IIncidentReportStatusListener.h>

extern sp<IBinder> generateInterface(string interfaceName, string varName);
#endif // FUZZER_INTERFACE