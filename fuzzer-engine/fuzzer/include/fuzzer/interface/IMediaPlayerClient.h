#ifndef IMEDIA_PLAYER_CLIENT
#define IMEDIA_PLAYER_CLIENT

#include <binder/ProcessState.h>
#include <cutils/properties.h> // for property_get

#include <media/DataSource.h>
#include <media/IMediaHTTPService.h>
#include <media/IStreamSource.h>
#include <media/MediaExtractor.h>
#include <media/MediaSource.h>
#include <media/mediaplayer.h>
#include <media/stagefright/DataSourceFactory.h>
#include <media/stagefright/InterfaceUtils.h>
#include <media/stagefright/MPEG2TSWriter.h>
#include <media/stagefright/MediaExtractorFactory.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AMessage.h>

#include <binder/IServiceManager.h>
#include <gui/ISurfaceComposer.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <media/IMediaPlayerService.h>

#include <fcntl.h>
#include <ui/DisplayInfo.h>

using namespace android;
extern sp<IBinder> generateIMediaPlayerClient();
#endif // IMEDIA_PLAYER_CLIENT