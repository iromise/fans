#ifndef IDRM_CLIENT_H
#define IDRM_CLIENT_H

#include <media/NdkMediaDrm.h>

#include <cutils/properties.h>
#include <gui/Surface.h>
#include <utils/Log.h>
#include <utils/StrongPointer.h>

#include <binder/IServiceManager.h>
#include <media/IDrm.h>
#include <media/IDrmClient.h>
#include <media/IMediaDrmService.h>
#include <media/NdkMediaCrypto.h>
#include <media/stagefright/MediaErrors.h>

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaCrypto.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaMuxer.h>

using namespace android;

extern sp<IBinder> generateIDrmClient();
#endif // IDRM_CLIENT_H