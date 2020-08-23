#include <fuzzer/interface/IOMX.h>

sp<IBinder> generateIOMX() {
  OMXClient client;
  if (client.connect() != OK) {
    ALOGE("Failed to connect to OMX.");
    return NULL;
  }
  sp<IOMX> omx = client.interface();
  return IInterface::asBinder(omx);
}