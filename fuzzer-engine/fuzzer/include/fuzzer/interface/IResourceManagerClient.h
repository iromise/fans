#ifndef RESOURCE_MANAGER_CLIENT_H
#define RESOURCE_MANAGER_CLIENT_H

#include <binder/IMemory.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/MemoryDealer.h>
#include <cutils/properties.h>
#include <fuzzer/types/types.h>
#include <gui/BufferQueue.h>
#include <gui/Surface.h>
#include <media/ICrypto.h>
#include <media/IOMX.h>
#include <media/IResourceManagerService.h>
#include <media/MediaAnalyticsItem.h>
#include <media/MediaCodecBuffer.h>
#include <media/stagefright/ACodec.h>
#include <media/stagefright/BufferProducerWrapper.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaCodecList.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaFilter.h>
#include <media/stagefright/OMXClient.h>
#include <media/stagefright/PersistentSurface.h>
#include <media/stagefright/SurfaceUtils.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/foundation/AUtils.h>
#include <media/stagefright/foundation/avc_utils.h>
#include <media/stagefright/foundation/hexdump.h>
#include <mediautils/BatteryNotifier.h>
#include <private/android_filesystem_config.h>
#include <utils/Singleton.h>
#include <utils/Vector.h>

extern Vector<AString> mimeVector;
extern Vector<AString> componentNameVector;

enum {
  kWhatInit = 'init',
  kWhatConfigure = 'conf',
  kWhatSetSurface = 'sSur',
  kWhatCreateInputSurface = 'cisf',
  kWhatSetInputSurface = 'sisf',
  kWhatStart = 'strt',
  kWhatStop = 'stop',
  kWhatRelease = 'rele',
  kWhatDequeueInputBuffer = 'deqI',
  kWhatQueueInputBuffer = 'queI',
  kWhatDequeueOutputBuffer = 'deqO',
  kWhatReleaseOutputBuffer = 'relO',
  kWhatSignalEndOfInputStream = 'eois',
  kWhatGetBuffers = 'getB',
  kWhatFlush = 'flus',
  kWhatGetOutputFormat = 'getO',
  kWhatGetInputFormat = 'getI',
  kWhatDequeueInputTimedOut = 'dITO',
  kWhatDequeueOutputTimedOut = 'dOTO',
  kWhatCodecNotify = 'codc',
  kWhatRequestIDRFrame = 'ridr',
  kWhatRequestActivityNotification = 'racN',
  kWhatGetName = 'getN',
  kWhatGetCodecInfo = 'gCoI',
  kWhatSetParameters = 'setP',
  kWhatSetCallback = 'setC',
  kWhatSetNotification = 'setN',
  kWhatDrmReleaseCrypto = 'rDrm',
};

extern void initCodecInfo();

extern sp<IBinder> generateIResourceManagerClient();
#endif // RESOURCE_MANAGER_CLIENT_H