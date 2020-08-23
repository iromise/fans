#ifndef DISPLAY_H
#define DISPLAY_H

#include <android/native_window.h>

#include <gui/ISurfaceComposer.h>
#include <gui/LayerState.h>

#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <private/gui/ComposerService.h>

#include <ui/DisplayInfo.h>
#include <ui/Rect.h>
#include <utils/String8.h>

using namespace android;
extern sp<IBinder> generateIDisplay();

#endif // DISPLAY_H