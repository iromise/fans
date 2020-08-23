#ifndef IMEDIA_RECORDER_CLIENT_H
#define IMEDIA_RECORDER_CLIENT_H

#include <inttypes.h>

#include <binder/IServiceManager.h>
#include <gui/IGraphicBufferProducer.h>
#include <media/IMediaPlayerService.h>
#include <media/IMediaRecorder.h>
#include <media/mediaplayer.h> // for MEDIA_ERROR_SERVER_DIED
#include <media/mediarecorder.h>
#include <media/stagefright/PersistentSurface.h>
#include <utils/Log.h>
#include <utils/String8.h>

using namespace android;
extern sp<IBinder> generateIMediaRecorderClient();
#endif // IMEDIA_RECORDER_CLIENT_H