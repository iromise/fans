#include <fuzzer/interface/IMediaRecorderClient.h>
#include <fuzzer/types/types.h>
sp<IBinder> generateIMediaRecorderClient() {
  String16 opPackageName(StringType::generatePackageName().c_str());
  sp<MediaRecorder> mediaRecorder = new MediaRecorder(opPackageName);
  return IInterface::asBinder(mediaRecorder);
}