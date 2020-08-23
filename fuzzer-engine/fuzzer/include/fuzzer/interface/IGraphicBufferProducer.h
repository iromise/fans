#ifndef IGRAPHIC_BUFFER_PRODUCER_H
#define IGRAPHIC_BUFFER_PRODUCER_H
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <fcntl.h>
#include <gui/IGraphicBufferProducer.h>
#include <media/IMediaPlayer.h>
#include <media/IMediaPlayerClient.h>
#include <media/IMediaPlayerService.h>
#include <media/IMediaRecorder.h>
#include <sys/stat.h>
#include <sys/time.h>

using namespace android;

extern sp<IBinder> generateIGraphicBufferProducer();
#endif // IGRAPHIC_BUFFER_PRODUCER_H