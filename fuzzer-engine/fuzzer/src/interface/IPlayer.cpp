#include <fuzzer/interface/IPlayer.h>

sp<IBinder> generateIPlayer() {
  sp<TrackPlayerBase> player = new TrackPlayerBase();
  return IInterface::asBinder(player);
}