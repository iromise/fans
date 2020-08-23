#include <fuzzer/interface/interface.h>

sp<IBinder> generateInterface(string interfaceName, string varName) {
  sp<IBinder> binder;
  if (interfaceName == "IAudioPolicyServiceClient") {
    binder = generateIAudioPolicyServiceClient();
  } else if (interfaceName == "IStreamSource") {
    binder = generateIStreamSource();
  } else if (interfaceName == "IMediaHTTPService") {
    binder = generateIMediaHTTPService();
  } else if (interfaceName == "IMediaPlayerClient") {
    binder = generateIMediaPlayerClient();
  } else if (interfaceName == "ISoundTriggerClient") {
    binder = generateISoundTriggerClient();
  } else if (interfaceName == "IAAudioClient") {
    binder = generateIAAudioClient();
  } else if (interfaceName == "IResourceManagerClient") {
    binder = generateIResourceManagerClient();
  } else if (interfaceName == "IDrmServiceListener") {
    binder = generateIDrmServiceListener();
  } else if (interfaceName == "IAudioFlingerClient") {
    binder = generateIAudioFlingerClient();
  } else if (interfaceName == "IEffectClient") {
    binder = generateIEffectClient();
  } else if (interfaceName == "IDrmClient") {
    binder = generateIDrmClient();
  } else if (interfaceName == "IMediaRecorderClient") {
    binder = generateIMediaRecorderClient();
  } else if (interfaceName == "ICameraRecordingProxy") {
    binder = generateICameraRecordingProxy();
  } else if (interfaceName == "ICameraClient") {
    binder = generateCamera();
  } else if (interfaceName == "ICameraDeviceCallbacks") {
    binder = generateICameraDeviceCallbacks();
  } else if (interfaceName == "ICameraRecordingProxyListener") {
    binder = generateICameraRecordingProxyListener();
  } else if (interfaceName == "ICamera") {
    binder = generateICamera();
  } else if (interfaceName == "IGraphicBufferProducer") {
    binder = generateIGraphicBufferProducer();
  } else if (interfaceName == "IAppOpsCallback") {
    binder = generateIAppOpsCallback();
  } else if (interfaceName == "IRemoteDisplayClient") {
    FUZZER_LOGI("Remote display client is no longer supported.");
    binder = NULL;
  } else if (interfaceName == "ICameraServiceListener") {
    FUZZER_LOGI("ICameraServiceListener is not supported now.");
    binder = NULL;
  } else if (interfaceName == "IGraphicBufferConsumer") {
    binder = generateIGraphicBufferConsumer();
  } else if (interfaceName == "IPlayer") {
    binder = generateIPlayer();
  } else if (interfaceName == "IOMX") {
    binder = generateIOMX();
  } else if (interfaceName == "IInterfaceEventCallback") {
    binder = generateIInterfaceEventCallback();
    // } else if (interfaceName == "IIncidentReportStatusListener") {
    //   binder = generateIIncidentReportStatusListener();
  } else if (interfaceName == "IBinder" && varName == "display") {
    binder = generateIDisplay();
  } else if (interfaceName == "IBinder") {
    binder = new BBinder();
  } else {
    binder = NULL;
    FUZZER_LOGI("Manaual generation of interface %s hasn't been supported.",
                interfaceName.c_str());
  }
  return binder;
}
