#include <fuzzer/interface/IDisplay.h>
#include <fuzzer/utils/random.h>
enum { eDisplayIdMain = 0, eDisplayIdHdmi = 1 };

sp<IBinder> generateIDisplay() {
  sp<ISurfaceComposer> sf(ComposerService::getComposerService());
  bool flag = randomUInt64(0, 1);
  sp<IBinder> display;
  if (flag) {
    display = sf->getBuiltInDisplay(ISurfaceComposer::eDisplayIdMain);
  } else {
    display = sf->getBuiltInDisplay(ISurfaceComposer::eDisplayIdHdmi);
  }
  return display;
}