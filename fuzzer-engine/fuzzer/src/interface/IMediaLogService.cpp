#include <fuzzer/interface/IMediaLogService.h>

sp<IBinder> generateIMediaLogService() {
  sp<MediaLogService> mediaLog = new MediaLogService();
  return IInterface::asBinder(mediaLog);
}