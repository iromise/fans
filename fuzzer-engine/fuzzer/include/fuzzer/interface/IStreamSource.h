#ifndef STREAM_SOURCE_H
#define STREAM_SOURCE_H

#include "sles_allinclusive.h"

#include "android/android_StreamPlayer.h"

#include <binder/IPCThreadState.h>
#include <media/IMediaPlayerService.h>
#include <media/IStreamSource.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/MediaKeys.h>

using namespace android;

extern sp<IBinder> generateIStreamSource();
#endif // STREAM_SOURCE_H