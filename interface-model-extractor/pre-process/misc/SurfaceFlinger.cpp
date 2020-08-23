/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// #define LOG_NDEBUG 0
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <stdint.h>
#include <sys/types.h>
#include <algorithm>
#include <errno.h>
#include <math.h>
#include <mutex>
#include <dlfcn.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <optional>

#include <cutils/properties.h>
#include <log/log.h>

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/PermissionCache.h>

#include <dvr/vr_flinger.h>

#include <ui/ColorSpace.h>
#include <ui/DebugUtils.h>
#include <ui/DisplayInfo.h>
#include <ui/DisplayStatInfo.h>

#include <gui/BufferQueue.h>
#include <gui/GuiConfig.h>
#include <gui/IDisplayEventConnection.h>
#include <gui/LayerDebugInfo.h>
#include <gui/Surface.h>

#include <ui/GraphicBufferAllocator.h>
#include <ui/PixelFormat.h>
#include <ui/UiConfig.h>

#include <utils/misc.h>
#include <utils/String8.h>
#include <utils/String16.h>
#include <utils/StopWatch.h>
#include <utils/Timers.h>
#include <utils/Trace.h>

#include <private/android_filesystem_config.h>
#include <private/gui/SyncFeatures.h>

#include "BufferLayer.h"
#include "Client.h"
#include "ColorLayer.h"
#include "Colorizer.h"
#include "ContainerLayer.h"
#include "DdmConnection.h"
#include "DispSync.h"
#include "DisplayDevice.h"
#include "EventControlThread.h"
#include "EventThread.h"
#include "Layer.h"
#include "LayerVector.h"
#include "MonitoredProducer.h"
#include "SurfaceFlinger.h"
#include "clz.h"

#include "DisplayHardware/ComposerHal.h"
#include "DisplayHardware/FramebufferSurface.h"
#include "DisplayHardware/HWComposer.h"
#include "DisplayHardware/VirtualDisplaySurface.h"

#include "Effects/Daltonizer.h"

#include "RenderEngine/RenderEngine.h"
#include <cutils/compiler.h>

#include <android/hardware/configstore/1.0/ISurfaceFlingerConfigs.h>
#include <android/hardware/configstore/1.1/ISurfaceFlingerConfigs.h>
#include <android/hardware/configstore/1.1/types.h>
#include <configstore/Utils.h>

#include <layerproto/LayerProtoParser.h>

#define DISPLAY_COUNT 1

/*
 * DEBUG_SCREENSHOTS: set to true to check that screenshots are not all
 * black pixels.
 */
#define DEBUG_SCREENSHOTS false

namespace android
{

using namespace android::hardware::configstore;
using namespace android::hardware::configstore::V1_0;
using ui::ColorMode;
using ui::Dataspace;
using ui::Hdr;
using ui::RenderIntent;

namespace
{
class ConditionalLock
{
public:
    ConditionalLock(Mutex &mutex, bool lock) : mMutex(mutex), mLocked(lock)
    {
        if (lock)
        {
            mMutex.lock();
        }
    }
    ~ConditionalLock()
    {
        if (mLocked)
            mMutex.unlock();
    }

private:
    Mutex &mMutex;
    bool mLocked;
};
} // namespace

// ---------------------------------------------------------------------------

const String16 sHardwareTest("android.permission.HARDWARE_TEST");
const String16 sAccessSurfaceFlinger("android.permission.ACCESS_SURFACE_FLINGER");
const String16 sReadFramebuffer("android.permission.READ_FRAME_BUFFER");
const String16 sDump("android.permission.DUMP");

// ---------------------------------------------------------------------------
int64_t SurfaceFlinger::vsyncPhaseOffsetNs;
int64_t SurfaceFlinger::sfVsyncPhaseOffsetNs;
int64_t SurfaceFlinger::dispSyncPresentTimeOffset;
bool SurfaceFlinger::useHwcForRgbToYuv;
uint64_t SurfaceFlinger::maxVirtualDisplaySize;
bool SurfaceFlinger::hasSyncFramework;
bool SurfaceFlinger::useVrFlinger;
int64_t SurfaceFlinger::maxFrameBufferAcquiredBuffers;
// TODO(courtneygo): Rename hasWideColorDisplay to clarify its actual meaning.
bool SurfaceFlinger::hasWideColorDisplay;

std::string getHwcServiceName()
{
    char value[PROPERTY_VALUE_MAX] = {};
    property_get("debug.sf.hwc_service_name", value, "default");
    ALOGI("Using HWComposer service: '%s'", value);
    return std::string(value);
}

bool useTrebleTestingOverride()
{
    char value[PROPERTY_VALUE_MAX] = {};
    property_get("debug.sf.treble_testing_override", value, "false");
    ALOGI("Treble testing override: '%s'", value);
    return std::string(value) == "true";
}

std::string decodeDisplayColorSetting(DisplayColorSetting displayColorSetting)
{
    switch (displayColorSetting)
    {
    case DisplayColorSetting::MANAGED:
        return std::string("Managed");
    case DisplayColorSetting::UNMANAGED:
        return std::string("Unmanaged");
    case DisplayColorSetting::ENHANCED:
        return std::string("Enhanced");
    default:
        return std::string("Unknown ") +
               std::to_string(static_cast<int>(displayColorSetting));
    }
}

NativeWindowSurface::~NativeWindowSurface() = default;

namespace impl
{

class NativeWindowSurface final : public android::NativeWindowSurface
{
public:
    static std::unique_ptr<android::NativeWindowSurface> create(
        const sp<IGraphicBufferProducer> &producer)
    {
        return std::make_unique<NativeWindowSurface>(producer);
    }

    explicit NativeWindowSurface(const sp<IGraphicBufferProducer> &producer)
        : surface(new Surface(producer, false)) {}

    ~NativeWindowSurface() override = default;

private:
    sp<ANativeWindow> getNativeWindow() const override { return surface; }

    void preallocateBuffers() override { surface->allocateBuffers(); }

    sp<Surface> surface;
};

} // namespace impl

SurfaceFlingerBE::SurfaceFlingerBE()
    : mHwcServiceName(getHwcServiceName()),
      mRenderEngine(nullptr),
      mFrameBuckets(),
      mTotalTime(0),
      mLastSwapTime(0),
      mComposerSequenceId(0)
{
}

SurfaceFlinger::SurfaceFlinger(SurfaceFlinger::SkipInitializationTag)
    : BnSurfaceComposer(),
      mTransactionFlags(0),
      mTransactionPending(false),
      mAnimTransactionPending(false),
      mLayersRemoved(false),
      mLayersAdded(false),
      mRepaintEverything(0),
      mBootTime(systemTime()),
      mBuiltinDisplays(),
      mVisibleRegionsDirty(false),
      mGeometryInvalid(false),
      mAnimCompositionPending(false),
      mBootStage(BootStage::BOOTLOADER),
      mDebugRegion(0),
      mDebugDDMS(0),
      mDebugDisableHWC(0),
      mDebugDisableTransformHint(0),
      mDebugInSwapBuffers(0),
      mLastSwapBufferTime(0),
      mDebugInTransaction(0),
      mLastTransactionTime(0),
      mForceFullDamage(false),
      mPrimaryDispSync("PrimaryDispSync"),
      mPrimaryHWVsyncEnabled(false),
      mHWVsyncAvailable(false),
      mHasPoweredOff(false),
      mNumLayers(0),
      mVrFlingerRequestsDisplay(false),
      mMainThreadId(std::this_thread::get_id()),
      mCreateBufferQueue(&BufferQueue::createBufferQueue),
      mCreateNativeWindowSurface(&impl::NativeWindowSurface::create) {}

SurfaceFlinger::SurfaceFlinger() : SurfaceFlinger(SkipInitialization)
{
    ALOGI("SurfaceFlinger is starting");

    vsyncPhaseOffsetNs = getInt64<ISurfaceFlingerConfigs,
                                  &ISurfaceFlingerConfigs::vsyncEventPhaseOffsetNs>(1000000);

    sfVsyncPhaseOffsetNs = getInt64<ISurfaceFlingerConfigs,
                                    &ISurfaceFlingerConfigs::vsyncSfEventPhaseOffsetNs>(1000000);

    hasSyncFramework = getBool<ISurfaceFlingerConfigs,
                               &ISurfaceFlingerConfigs::hasSyncFramework>(true);

    dispSyncPresentTimeOffset = getInt64<ISurfaceFlingerConfigs,
                                         &ISurfaceFlingerConfigs::presentTimeOffsetFromVSyncNs>(0);

    useHwcForRgbToYuv = getBool<ISurfaceFlingerConfigs,
                                &ISurfaceFlingerConfigs::useHwcForRGBtoYUV>(false);

    maxVirtualDisplaySize = getUInt64<ISurfaceFlingerConfigs,
                                      &ISurfaceFlingerConfigs::maxVirtualDisplaySize>(0);

    // Vr flinger is only enabled on Daydream ready devices.
    useVrFlinger = getBool<ISurfaceFlingerConfigs,
                           &ISurfaceFlingerConfigs::useVrFlinger>(false);

    maxFrameBufferAcquiredBuffers = getInt64<ISurfaceFlingerConfigs,
                                             &ISurfaceFlingerConfigs::maxFrameBufferAcquiredBuffers>(2);

    hasWideColorDisplay =
        getBool<ISurfaceFlingerConfigs, &ISurfaceFlingerConfigs::hasWideColorDisplay>(false);

    V1_1::DisplayOrientation primaryDisplayOrientation =
        getDisplayOrientation<V1_1::ISurfaceFlingerConfigs, &V1_1::ISurfaceFlingerConfigs::primaryDisplayOrientation>(
            V1_1::DisplayOrientation::ORIENTATION_0);

    switch (primaryDisplayOrientation)
    {
    case V1_1::DisplayOrientation::ORIENTATION_90:
        mPrimaryDisplayOrientation = DisplayState::eOrientation90;
        break;
    case V1_1::DisplayOrientation::ORIENTATION_180:
        mPrimaryDisplayOrientation = DisplayState::eOrientation180;
        break;
    case V1_1::DisplayOrientation::ORIENTATION_270:
        mPrimaryDisplayOrientation = DisplayState::eOrientation270;
        break;
    default:
        mPrimaryDisplayOrientation = DisplayState::eOrientationDefault;
        break;
    }
    ALOGV("Primary Display Orientation is set to %2d.", mPrimaryDisplayOrientation);

    mPrimaryDispSync.init(SurfaceFlinger::hasSyncFramework, SurfaceFlinger::dispSyncPresentTimeOffset);

    // debugging stuff...
    char value[PROPERTY_VALUE_MAX];

    property_get("ro.bq.gpu_to_cpu_unsupported", value, "0");
    mGpuToCpuSupported = !atoi(value);

    property_get("debug.sf.showupdates", value, "0");
    mDebugRegion = atoi(value);

    property_get("debug.sf.ddms", value, "0");
    mDebugDDMS = atoi(value);
    if (mDebugDDMS)
    {
        if (!startDdmConnection())
        {
            // start failed, and DDMS debugging not enabled
            mDebugDDMS = 0;
        }
    }
    ALOGI_IF(mDebugRegion, "showupdates enabled");
    ALOGI_IF(mDebugDDMS, "DDMS debugging enabled");

    property_get("debug.sf.disable_backpressure", value, "0");
    mPropagateBackpressure = !atoi(value);
    ALOGI_IF(!mPropagateBackpressure, "Disabling backpressure propagation");

    property_get("debug.sf.enable_hwc_vds", value, "0");
    mUseHwcVirtualDisplays = atoi(value);
    ALOGI_IF(!mUseHwcVirtualDisplays, "Enabling HWC virtual displays");

    property_get("ro.sf.disable_triple_buffer", value, "1");
    mLayerTripleBufferingDisabled = atoi(value);
    ALOGI_IF(mLayerTripleBufferingDisabled, "Disabling Triple Buffering");

    const size_t defaultListSize = MAX_LAYERS;
    auto listSize = property_get_int32("debug.sf.max_igbp_list_size", int32_t(defaultListSize));
    mMaxGraphicBufferProducerListSize = (listSize > 0) ? size_t(listSize) : defaultListSize;

    property_get("debug.sf.early_phase_offset_ns", value, "-1");
    const int earlySfOffsetNs = atoi(value);

    property_get("debug.sf.early_gl_phase_offset_ns", value, "-1");
    const int earlyGlSfOffsetNs = atoi(value);

    property_get("debug.sf.early_app_phase_offset_ns", value, "-1");
    const int earlyAppOffsetNs = atoi(value);

    property_get("debug.sf.early_gl_app_phase_offset_ns", value, "-1");
    const int earlyGlAppOffsetNs = atoi(value);

    const VSyncModulator::Offsets earlyOffsets =
        {earlySfOffsetNs != -1 ? earlySfOffsetNs : sfVsyncPhaseOffsetNs,
         earlyAppOffsetNs != -1 ? earlyAppOffsetNs : vsyncPhaseOffsetNs};
    const VSyncModulator::Offsets earlyGlOffsets =
        {earlyGlSfOffsetNs != -1 ? earlyGlSfOffsetNs : sfVsyncPhaseOffsetNs,
         earlyGlAppOffsetNs != -1 ? earlyGlAppOffsetNs : vsyncPhaseOffsetNs};
    mVsyncModulator.setPhaseOffsets(earlyOffsets, earlyGlOffsets,
                                    {sfVsyncPhaseOffsetNs, vsyncPhaseOffsetNs});

    // We should be reading 'persist.sys.sf.color_saturation' here
    // but since /data may be encrypted, we need to wait until after vold
    // comes online to attempt to read the property. The property is
    // instead read after the boot animation

    if (useTrebleTestingOverride())
    {
        // Without the override SurfaceFlinger cannot connect to HIDL
        // services that are not listed in the manifests.  Considered
        // deriving the setting from the set service name, but it
        // would be brittle if the name that's not 'default' is used
        // for production purposes later on.
        setenv("TREBLE_TESTING_OVERRIDE", "true", true);
    }
}

void SurfaceFlinger::onFirstRef()
{
    mEventQueue->init(this);
}

SurfaceFlinger::~SurfaceFlinger()
{
}

void SurfaceFlinger::binderDied(const wp<IBinder> & /* who */)
{
    // the window manager died on us. prepare its eulogy.

    // restore initial conditions (default device unblank, etc)
    initializeDisplays();

    // restart the boot-animation
    startBootAnim();
}

static sp<ISurfaceComposerClient> initClient(const sp<Client> &client)
{
    status_t err = client->initCheck();
    if (err == NO_ERROR)
    {
        return client;
    }
    return nullptr;
}

sp<ISurfaceComposerClient> SurfaceFlinger::createConnection()
{
    return initClient(new Client(this));
}

sp<ISurfaceComposerClient> SurfaceFlinger::createScopedConnection(
    const sp<IGraphicBufferProducer> &gbp)
{
    if (authenticateSurfaceTexture(gbp) == false)
    {
        return nullptr;
    }
    const auto &layer = (static_cast<MonitoredProducer *>(gbp.get()))->getLayer();
    if (layer == nullptr)
    {
        return nullptr;
    }

    return initClient(new Client(this, layer));
}

sp<IBinder> SurfaceFlinger::createDisplay(const String8 &displayName,
                                          bool secure)
{
    class DisplayToken : public BBinder
    {
        sp<SurfaceFlinger> flinger;
        virtual ~DisplayToken()
        {
            // no more references, this display must be terminated
            Mutex::Autolock _l(flinger->mStateLock);
            flinger->mCurrentState.displays.removeItem(this);
            flinger->setTransactionFlags(eDisplayTransactionNeeded);
        }

    public:
        explicit DisplayToken(const sp<SurfaceFlinger> &flinger)
            : flinger(flinger)
        {
        }
    };

    sp<BBinder> token = new DisplayToken(this);

    Mutex::Autolock _l(mStateLock);
    DisplayDeviceState info(DisplayDevice::DISPLAY_VIRTUAL, secure);
    info.displayName = displayName;
    mCurrentState.displays.add(token, info);
    mInterceptor->saveDisplayCreation(info);
    return token;
}

void SurfaceFlinger::destroyDisplay(const sp<IBinder> &display)
{
    Mutex::Autolock _l(mStateLock);

    ssize_t idx = mCurrentState.displays.indexOfKey(display);
    if (idx < 0)
    {
        ALOGW("destroyDisplay: invalid display token");
        return;
    }

    const DisplayDeviceState &info(mCurrentState.displays.valueAt(idx));
    if (!info.isVirtualDisplay())
    {
        ALOGE("destroyDisplay called for non-virtual display");
        return;
    }
    mInterceptor->saveDisplayDeletion(info.displayId);
    mCurrentState.displays.removeItemsAt(idx);
    setTransactionFlags(eDisplayTransactionNeeded);
}

sp<IBinder> SurfaceFlinger::getBuiltInDisplay(int32_t id)
{
    if (uint32_t(id) >= DisplayDevice::NUM_BUILTIN_DISPLAY_TYPES)
    {
        ALOGE("getDefaultDisplay: id=%d is not a valid default display id", id);
        return nullptr;
    }
    return mBuiltinDisplays[id];
}

void SurfaceFlinger::bootFinished()
{
    if (mStartPropertySetThread->join() != NO_ERROR)
    {
        ALOGE("Join StartPropertySetThread failed!");
    }
    const nsecs_t now = systemTime();
    const nsecs_t duration = now - mBootTime;
    ALOGI("Boot is finished (%ld ms)", long(ns2ms(duration)));

    // wait patiently for the window manager death
    const String16 name("window");
    sp<IBinder> window(defaultServiceManager()->getService(name));
    if (window != 0)
    {
        window->linkToDeath(static_cast<IBinder::DeathRecipient *>(this));
    }

    if (mVrFlinger)
    {
        mVrFlinger->OnBootFinished();
    }

    // stop boot animation
    // formerly we would just kill the process, but we now ask it to exit so it
    // can choose where to stop the animation.
    property_set("service.bootanim.exit", "1");

    const int LOGTAG_SF_STOP_BOOTANIM = 60110;
    LOG_EVENT_LONG(LOGTAG_SF_STOP_BOOTANIM,
                   ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)));

    sp<LambdaMessage> readProperties = new LambdaMessage([&]() {
        readPersistentProperties();
        mBootStage = BootStage::FINISHED;
    });
    postMessageAsync(readProperties);
}

uint32_t SurfaceFlinger::getNewTexture()
{
    {
        std::lock_guard lock(mTexturePoolMutex);
        if (!mTexturePool.empty())
        {
            uint32_t name = mTexturePool.back();
            mTexturePool.pop_back();
            ATRACE_INT("TexturePoolSize", mTexturePool.size());
            return name;
        }

        // The pool was too small, so increase it for the future
        ++mTexturePoolSize;
    }

    // The pool was empty, so we need to get a new texture name directly using a
    // blocking call to the main thread
    uint32_t name = 0;
    postMessageSync(new LambdaMessage([&]() { getRenderEngine().genTextures(1, &name); }));
    return name;
}

void SurfaceFlinger::deleteTextureAsync(uint32_t texture)
{
    class MessageDestroyGLTexture : public MessageBase
    {
        RE::RenderEngine &engine;
        uint32_t texture;

    public:
        MessageDestroyGLTexture(RE::RenderEngine &engine, uint32_t texture)
            : engine(engine), texture(texture) {}
        virtual bool handler()
        {
            engine.deleteTextures(1, &texture);
            return true;
        }
    };
    postMessageAsync(new MessageDestroyGLTexture(getRenderEngine(), texture));
}

class DispSyncSource final : public VSyncSource, private DispSync::Callback
{
public:
    DispSyncSource(DispSync *dispSync, nsecs_t phaseOffset, bool traceVsync,
                   const char *name) : mName(name),
                                       mValue(0),
                                       mTraceVsync(traceVsync),
                                       mVsyncOnLabel(String8::format("VsyncOn-%s", name)),
                                       mVsyncEventLabel(String8::format("VSYNC-%s", name)),
                                       mDispSync(dispSync),
                                       mCallbackMutex(),
                                       mVsyncMutex(),
                                       mPhaseOffset(phaseOffset),
                                       mEnabled(false) {}

    ~DispSyncSource() override = default;

    void setVSyncEnabled(bool enable) override
    {
        Mutex::Autolock lock(mVsyncMutex);
        if (enable)
        {
            status_t err = mDispSync->addEventListener(mName, mPhaseOffset,
                                                       static_cast<DispSync::Callback *>(this));
            if (err != NO_ERROR)
            {
                ALOGE("error registering vsync callback: %s (%d)",
                      strerror(-err), err);
            }
            //ATRACE_INT(mVsyncOnLabel.string(), 1);
        }
        else
        {
            status_t err = mDispSync->removeEventListener(
                static_cast<DispSync::Callback *>(this));
            if (err != NO_ERROR)
            {
                ALOGE("error unregistering vsync callback: %s (%d)",
                      strerror(-err), err);
            }
            //ATRACE_INT(mVsyncOnLabel.string(), 0);
        }
        mEnabled = enable;
    }

    void setCallback(VSyncSource::Callback *callback) override
    {
        Mutex::Autolock lock(mCallbackMutex);
        mCallback = callback;
    }

    void setPhaseOffset(nsecs_t phaseOffset) override
    {
        Mutex::Autolock lock(mVsyncMutex);

        // Normalize phaseOffset to [0, period)
        auto period = mDispSync->getPeriod();
        phaseOffset %= period;
        if (phaseOffset < 0)
        {
            // If we're here, then phaseOffset is in (-period, 0). After this
            // operation, it will be in (0, period)
            phaseOffset += period;
        }
        mPhaseOffset = phaseOffset;

        // If we're not enabled, we don't need to mess with the listeners
        if (!mEnabled)
        {
            return;
        }

        status_t err = mDispSync->changePhaseOffset(static_cast<DispSync::Callback *>(this),
                                                    mPhaseOffset);
        if (err != NO_ERROR)
        {
            ALOGE("error changing vsync offset: %s (%d)",
                  strerror(-err), err);
        }
    }

private:
    virtual void onDispSyncEvent(nsecs_t when)
    {
        VSyncSource::Callback *callback;
        {
            Mutex::Autolock lock(mCallbackMutex);
            callback = mCallback;

            if (mTraceVsync)
            {
                mValue = (mValue + 1) % 2;
                ATRACE_INT(mVsyncEventLabel.string(), mValue);
            }
        }

        if (callback != nullptr)
        {
            callback->onVSyncEvent(when);
        }
    }

    const char *const mName;

    int mValue;

    const bool mTraceVsync;
    const String8 mVsyncOnLabel;
    const String8 mVsyncEventLabel;

    DispSync *mDispSync;

    Mutex mCallbackMutex; // Protects the following
    VSyncSource::Callback *mCallback = nullptr;

    Mutex mVsyncMutex; // Protects the following
    nsecs_t mPhaseOffset;
    bool mEnabled;
};

class InjectVSyncSource final : public VSyncSource
{
public:
    InjectVSyncSource() = default;
    ~InjectVSyncSource() override = default;

    void setCallback(VSyncSource::Callback *callback) override
    {
        std::lock_guard<std::mutex> lock(mCallbackMutex);
        mCallback = callback;
    }

    void onInjectSyncEvent(nsecs_t when)
    {
        std::lock_guard<std::mutex> lock(mCallbackMutex);
        if (mCallback)
        {
            mCallback->onVSyncEvent(when);
        }
    }

    void setVSyncEnabled(bool) override {}
    void setPhaseOffset(nsecs_t) override {}

private:
    std::mutex mCallbackMutex; // Protects the following
    VSyncSource::Callback *mCallback = nullptr;
};

// Do not call property_set on main thread which will be blocked by init
// Use StartPropertySetThread instead.
void SurfaceFlinger::init()
{
    ALOGI("SurfaceFlinger's main thread ready to run. "
          "Initializing graphics H/W...");

    ALOGI("Phase offest NS: %" PRId64 "", vsyncPhaseOffsetNs);

    Mutex::Autolock _l(mStateLock);

    // start the EventThread
    mEventThreadSource =
        std::make_unique<DispSyncSource>(&mPrimaryDispSync, SurfaceFlinger::vsyncPhaseOffsetNs,
                                         true, "app");
    mEventThread = std::make_unique<impl::EventThread>(
        mEventThreadSource.get(),
        [this]() { resyncWithRateLimit(); },
        impl::EventThread::InterceptVSyncsCallback(),
        "appEventThread");
    mSfEventThreadSource =
        std::make_unique<DispSyncSource>(&mPrimaryDispSync,
                                         SurfaceFlinger::sfVsyncPhaseOffsetNs, true, "sf");

    mSFEventThread =
        std::make_unique<impl::EventThread>(
            mSfEventThreadSource.get(),
            [this]() { resyncWithRateLimit(); },
            [this](nsecs_t timestamp) {
                mInterceptor->saveVSyncEvent(timestamp);
            },
            "sfEventThread");
    mEventQueue->setEventThread(mSFEventThread.get());
    mVsyncModulator.setEventThreads(mSFEventThread.get(), mEventThread.get());

    // Get a RenderEngine for the given display / config (can't fail)
    getBE().mRenderEngine =
        RE::impl::RenderEngine::create(HAL_PIXEL_FORMAT_RGBA_8888,
                                       hasWideColorDisplay
                                           ? RE::RenderEngine::WIDE_COLOR_SUPPORT
                                           : 0);
    LOG_ALWAYS_FATAL_IF(getBE().mRenderEngine == nullptr, "couldn't create RenderEngine");

    LOG_ALWAYS_FATAL_IF(mVrFlingerRequestsDisplay,
                        "Starting with vr flinger active is not currently supported.");
    getBE().mHwc.reset(
        new HWComposer(std::make_unique<Hwc2::impl::Composer>(getBE().mHwcServiceName)));
    getBE().mHwc->registerCallback(this, getBE().mComposerSequenceId);
    // Process any initial hotplug and resulting display changes.
    processDisplayHotplugEventsLocked();
    LOG_ALWAYS_FATAL_IF(!getBE().mHwc->isConnected(HWC_DISPLAY_PRIMARY),
                        "Registered composer callback but didn't create the default primary display");

    // make the default display GLContext current so that we can create textures
    // when creating Layers (which may happens before we render something)
    getDefaultDisplayDeviceLocked()->makeCurrent();

    if (useVrFlinger)
    {
        auto vrFlingerRequestDisplayCallback = [this](bool requestDisplay) {
            // This callback is called from the vr flinger dispatch thread. We
            // need to call signalTransaction(), which requires holding
            // mStateLock when we're not on the main thread. Acquiring
            // mStateLock from the vr flinger dispatch thread might trigger a
            // deadlock in surface flinger (see b/66916578), so post a message
            // to be handled on the main thread instead.
            sp<LambdaMessage> message = new LambdaMessage([=]() {
                ALOGI("VR request display mode: requestDisplay=%d", requestDisplay);
                mVrFlingerRequestsDisplay = requestDisplay;
                signalTransaction();
            });
            postMessageAsync(message);
        };
        mVrFlinger = dvr::VrFlinger::Create(getBE().mHwc->getComposer(),
                                            getBE().mHwc->getHwcDisplayId(HWC_DISPLAY_PRIMARY).value_or(0),
                                            vrFlingerRequestDisplayCallback);
        if (!mVrFlinger)
        {
            ALOGE("Failed to start vrflinger");
        }
    }

    mEventControlThread = std::make_unique<impl::EventControlThread>(
        [this](bool enabled) { setVsyncEnabled(HWC_DISPLAY_PRIMARY, enabled); });

    // initialize our drawing state
    mDrawingState = mCurrentState;

    // set initial conditions (e.g. unblank default device)
    initializeDisplays();

    getBE().mRenderEngine->primeCache();

    // Inform native graphics APIs whether the present timestamp is supported:
    if (getHwComposer().hasCapability(
            HWC2::Capability::PresentFenceIsNotReliable))
    {
        mStartPropertySetThread = new StartPropertySetThread(false);
    }
    else
    {
        mStartPropertySetThread = new StartPropertySetThread(true);
    }

    if (mStartPropertySetThread->Start() != NO_ERROR)
    {
        ALOGE("Run StartPropertySetThread failed!");
    }

    // This is a hack. Per definition of getDataspaceSaturationMatrix, the returned matrix
    // is used to saturate legacy sRGB content. However, to make sure the same color under
    // Display P3 will be saturated to the same color, we intentionally break the API spec
    // and apply this saturation matrix on Display P3 content. Unless the risk of applying
    // such saturation matrix on Display P3 is understood fully, the API should always return
    // identify matrix.
    mEnhancedSaturationMatrix = getBE().mHwc->getDataspaceSaturationMatrix(HWC_DISPLAY_PRIMARY,
                                                                           Dataspace::SRGB_LINEAR);

    // we will apply this on Display P3.
    if (mEnhancedSaturationMatrix != mat4())
    {
        ColorSpace srgb(ColorSpace::sRGB());
        ColorSpace displayP3(ColorSpace::DisplayP3());
        mat4 srgbToP3 = mat4(ColorSpaceConnector(srgb, displayP3).getTransform());
        mat4 p3ToSrgb = mat4(ColorSpaceConnector(displayP3, srgb).getTransform());
        mEnhancedSaturationMatrix = srgbToP3 * mEnhancedSaturationMatrix * p3ToSrgb;
    }

    ALOGV("Done initializing");
}

void SurfaceFlinger::readPersistentProperties()
{
    Mutex::Autolock _l(mStateLock);

    char value[PROPERTY_VALUE_MAX];

    property_get("persist.sys.sf.color_saturation", value, "1.0");
    mGlobalSaturationFactor = atof(value);
    updateColorMatrixLocked();
    ALOGV("Saturation is set to %.2f", mGlobalSaturationFactor);

    property_get("persist.sys.sf.native_mode", value, "0");
    mDisplayColorSetting = static_cast<DisplayColorSetting>(atoi(value));
}

void SurfaceFlinger::startBootAnim()
{
    // Start boot animation service by setting a property mailbox
    // if property setting thread is already running, Start() will be just a NOP
    mStartPropertySetThread->Start();
    // Wait until property was set
    if (mStartPropertySetThread->join() != NO_ERROR)
    {
        ALOGE("Join StartPropertySetThread failed!");
    }
}

size_t SurfaceFlinger::getMaxTextureSize() const
{
    return getBE().mRenderEngine->getMaxTextureSize();
}

size_t SurfaceFlinger::getMaxViewportDims() const
{
    return getBE().mRenderEngine->getMaxViewportDims();
}

// ----------------------------------------------------------------------------

bool SurfaceFlinger::authenticateSurfaceTexture(
    const sp<IGraphicBufferProducer> &bufferProducer) const
{
    Mutex::Autolock _l(mStateLock);
    return authenticateSurfaceTextureLocked(bufferProducer);
}

bool SurfaceFlinger::authenticateSurfaceTextureLocked(
    const sp<IGraphicBufferProducer> &bufferProducer) const
{
    sp<IBinder> surfaceTextureBinder(IInterface::asBinder(bufferProducer));
    return mGraphicBufferProducerList.count(surfaceTextureBinder.get()) > 0;
}

status_t SurfaceFlinger::getSupportedFrameTimestamps(
    std::vector<FrameEvent> *outSupported) const
{
    *outSupported = {
        FrameEvent::REQUESTED_PRESENT,
        FrameEvent::ACQUIRE,
        FrameEvent::LATCH,
        FrameEvent::FIRST_REFRESH_START,
        FrameEvent::LAST_REFRESH_START,
        FrameEvent::GPU_COMPOSITION_DONE,
        FrameEvent::DEQUEUE_READY,
        FrameEvent::RELEASE,
    };
    ConditionalLock _l(mStateLock,
                       std::this_thread::get_id() != mMainThreadId);
    if (!getHwComposer().hasCapability(
            HWC2::Capability::PresentFenceIsNotReliable))
    {
        outSupported->push_back(FrameEvent::DISPLAY_PRESENT);
    }
    return NO_ERROR;
}

status_t SurfaceFlinger::getDisplayConfigs(const sp<IBinder> &display,
                                           Vector<DisplayInfo> *configs)
{
    if (configs == nullptr || display.get() == nullptr)
    {
        return BAD_VALUE;
    }

    if (!display.get())
        return NAME_NOT_FOUND;

    int32_t type = NAME_NOT_FOUND;
    for (int i = 0; i < DisplayDevice::NUM_BUILTIN_DISPLAY_TYPES; i++)
    {
        if (display == mBuiltinDisplays[i])
        {
            type = i;
            break;
        }
    }

    if (type < 0)
    {
        return type;
    }

    // TODO: Not sure if display density should handled by SF any longer
    class Density
    {
        static int getDensityFromProperty(char const *propName)
        {
            char property[PROPERTY_VALUE_MAX];
            int density = 0;
            if (property_get(propName, property, nullptr) > 0)
            {
                density = atoi(property);
            }
            return density;
        }

    public:
        static int getEmuDensity()
        {
            return getDensityFromProperty("qemu.sf.lcd_density");
        }
        static int getBuildDensity()
        {
            return getDensityFromProperty("ro.sf.lcd_density");
        }
    };

    configs->clear();

    ConditionalLock _l(mStateLock,
                       std::this_thread::get_id() != mMainThreadId);
    for (const auto &hwConfig : getHwComposer().getConfigs(type))
    {
        DisplayInfo info = DisplayInfo();

        float xdpi = hwConfig->getDpiX();
        float ydpi = hwConfig->getDpiY();

        if (type == DisplayDevice::DISPLAY_PRIMARY)
        {
            // The density of the device is provided by a build property
            float density = Density::getBuildDensity() / 160.0f;
            if (density == 0)
            {
                // the build doesn't provide a density -- this is wrong!
                // use xdpi instead
                ALOGE("ro.sf.lcd_density must be defined as a build property");
                density = xdpi / 160.0f;
            }
            if (Density::getEmuDensity())
            {
                // if "qemu.sf.lcd_density" is specified, it overrides everything
                xdpi = ydpi = density = Density::getEmuDensity();
                density /= 160.0f;
            }
            info.density = density;

            // TODO: this needs to go away (currently needed only by webkit)
            sp<const DisplayDevice> hw(getDefaultDisplayDeviceLocked());
            info.orientation = hw ? hw->getOrientation() : 0;
        }
        else
        {
            // TODO: where should this value come from?
            static const int TV_DENSITY = 213;
            info.density = TV_DENSITY / 160.0f;
            info.orientation = 0;
        }

        info.w = hwConfig->getWidth();
        info.h = hwConfig->getHeight();
        info.xdpi = xdpi;
        info.ydpi = ydpi;
        info.fps = 1e9 / hwConfig->getVsyncPeriod();
        info.appVsyncOffset = vsyncPhaseOffsetNs;

        // This is how far in advance a buffer must be queued for
        // presentation at a given time.  If you want a buffer to appear
        // on the screen at time N, you must submit the buffer before
        // (N - presentationDeadline).
        //
        // Normally it's one full refresh period (to give SF a chance to
        // latch the buffer), but this can be reduced by configuring a
        // DispSync offset.  Any additional delays introduced by the hardware
        // composer or panel must be accounted for here.
        //
        // We add an additional 1ms to allow for processing time and
        // differences between the ideal and actual refresh rate.
        info.presentationDeadline = hwConfig->getVsyncPeriod() -
                                    sfVsyncPhaseOffsetNs + 1000000;

        // All non-virtual displays are currently considered secure.
        info.secure = true;

        if (type == DisplayDevice::DISPLAY_PRIMARY &&
            mPrimaryDisplayOrientation & DisplayState::eOrientationSwapMask)
        {
            std::swap(info.w, info.h);
        }

        configs->push_back(info);
    }

    return NO_ERROR;
}

status_t SurfaceFlinger::getDisplayStats(const sp<IBinder> & /* display */,
                                         DisplayStatInfo *stats)
{
    if (stats == nullptr)
    {
        return BAD_VALUE;
    }

    // FIXME for now we always return stats for the primary display
    memset(stats, 0, sizeof(*stats));
    stats->vsyncTime = mPrimaryDispSync.computeNextRefresh(0);
    stats->vsyncPeriod = mPrimaryDispSync.getPeriod();
    return NO_ERROR;
}

status_t SurfaceFlinger::getDisplayViewport(const sp<IBinder> &display, Rect *outViewport)
{
    if (outViewport == nullptr || display.get() == nullptr)
    {
        return BAD_VALUE;
    }

    sp<const DisplayDevice> device(getDisplayDevice(display));
    if (device == nullptr)
    {
        return BAD_VALUE;
    }

    *outViewport = device->getViewport();

    return NO_ERROR;
}

int SurfaceFlinger::getActiveConfig(const sp<IBinder> &display)
{
    if (display == nullptr)
    {
        ALOGE("%s : display is nullptr", __func__);
        return BAD_VALUE;
    }

    sp<const DisplayDevice> device(getDisplayDevice(display));
    if (device != nullptr)
    {
        return device->getActiveConfig();
    }

    return BAD_VALUE;
}

void SurfaceFlinger::setActiveConfigInternal(const sp<DisplayDevice> &hw, int mode)
{
    ALOGD("Set active config mode=%d, type=%d flinger=%p", mode, hw->getDisplayType(),
          this);
    int32_t type = hw->getDisplayType();
    int currentMode = hw->getActiveConfig();

    if (mode == currentMode)
    {
        ALOGD("Screen type=%d is already mode=%d", hw->getDisplayType(), mode);
        return;
    }

    if (type >= DisplayDevice::NUM_BUILTIN_DISPLAY_TYPES)
    {
        ALOGW("Trying to set config for virtual display");
        return;
    }

    hw->setActiveConfig(mode);
    getHwComposer().setActiveConfig(type, mode);
}

status_t SurfaceFlinger::setActiveConfig(const sp<IBinder> &display, int mode)
{
    class MessageSetActiveConfig : public MessageBase
    {
        SurfaceFlinger &mFlinger;
        sp<IBinder> mDisplay;
        int mMode;

    public:
        MessageSetActiveConfig(SurfaceFlinger &flinger, const sp<IBinder> &disp,
                               int mode) : mFlinger(flinger), mDisplay(disp) { mMode = mode; }
        virtual bool handler()
        {
            Vector<DisplayInfo> configs;
            mFlinger.getDisplayConfigs(mDisplay, &configs);
            if (mMode < 0 || mMode >= static_cast<int>(configs.size()))
            {
                ALOGE("Attempt to set active config = %d for display with %zu configs",
                      mMode, configs.size());
                return true;
            }
            sp<DisplayDevice> hw(mFlinger.getDisplayDevice(mDisplay));
            if (hw == nullptr)
            {
                ALOGE("Attempt to set active config = %d for null display %p",
                      mMode, mDisplay.get());
            }
            else if (hw->getDisplayType() >= DisplayDevice::DISPLAY_VIRTUAL)
            {
                ALOGW("Attempt to set active config = %d for virtual display",
                      mMode);
            }
            else
            {
                mFlinger.setActiveConfigInternal(hw, mMode);
            }
            return true;
        }
    };
    sp<MessageBase> msg = new MessageSetActiveConfig(*this, display, mode);
    postMessageSync(msg);
    return NO_ERROR;
}
status_t SurfaceFlinger::getDisplayColorModes(const sp<IBinder> &display,
                                              Vector<ColorMode> *outColorModes)
{
    if ((outColorModes == nullptr) || (display.get() == nullptr))
    {
        return BAD_VALUE;
    }

    if (!display.get())
    {
        return NAME_NOT_FOUND;
    }

    int32_t type = NAME_NOT_FOUND;
    for (int i = 0; i < DisplayDevice::NUM_BUILTIN_DISPLAY_TYPES; i++)
    {
        if (display == mBuiltinDisplays[i])
        {
            type = i;
            break;
        }
    }

    if (type < 0)
    {
        return type;
    }

    std::vector<ColorMode> modes;
    {
        ConditionalLock _l(mStateLock,
                           std::this_thread::get_id() != mMainThreadId);
        modes = getHwComposer().getColorModes(type);
    }
    outColorModes->clear();
    std::copy(modes.cbegin(), modes.cend(), std::back_inserter(*outColorModes));

    return NO_ERROR;
}

ColorMode SurfaceFlinger::getActiveColorMode(const sp<IBinder> &display)
{
    sp<const DisplayDevice> device(getDisplayDevice(display));
    if (device != nullptr)
    {
        return device->getActiveColorMode();
    }
    return static_cast<ColorMode>(BAD_VALUE);
}

void SurfaceFlinger::setActiveColorModeInternal(const sp<DisplayDevice> &hw,
                                                ColorMode mode, Dataspace dataSpace,
                                                RenderIntent renderIntent)
{
    int32_t type = hw->getDisplayType();
    ColorMode currentMode = hw->getActiveColorMode();
    Dataspace currentDataSpace = hw->getCompositionDataSpace();
    RenderIntent currentRenderIntent = hw->getActiveRenderIntent();

    if (mode == currentMode && dataSpace == currentDataSpace &&
        renderIntent == currentRenderIntent)
    {
        return;
    }

    if (type >= DisplayDevice::NUM_BUILTIN_DISPLAY_TYPES)
    {
        ALOGW("Trying to set config for virtual display");
        return;
    }

    hw->setActiveColorMode(mode);
    hw->setCompositionDataSpace(dataSpace);
    hw->setActiveRenderIntent(renderIntent);
    getHwComposer().setActiveColorMode(type, mode, renderIntent);

    ALOGV("Set active color mode: %s (%d), active render intent: %s (%d), type=%d",
          decodeColorMode(mode).c_str(), mode,
          decodeRenderIntent(renderIntent).c_str(), renderIntent,
          hw->getDisplayType());
}

status_t SurfaceFlinger::setActiveColorMode(const sp<IBinder> &display,
                                            ColorMode colorMode)
{
    class MessageSetActiveColorMode : public MessageBase
    {
        SurfaceFlinger &mFlinger;
        sp<IBinder> mDisplay;
        ColorMode mMode;

    public:
        MessageSetActiveColorMode(SurfaceFlinger &flinger, const sp<IBinder> &disp,
                                  ColorMode mode) : mFlinger(flinger), mDisplay(disp) { mMode = mode; }
        virtual bool handler()
        {
            Vector<ColorMode> modes;
            mFlinger.getDisplayColorModes(mDisplay, &modes);
            bool exists = std::find(std::begin(modes), std::end(modes), mMode) != std::end(modes);
            if (mMode < ColorMode::NATIVE || !exists)
            {
                ALOGE("Attempt to set invalid active color mode %s (%d) for display %p",
                      decodeColorMode(mMode).c_str(), mMode, mDisplay.get());
                return true;
            }
            sp<DisplayDevice> hw(mFlinger.getDisplayDevice(mDisplay));
            if (hw == nullptr)
            {
                ALOGE("Attempt to set active color mode %s (%d) for null display %p",
                      decodeColorMode(mMode).c_str(), mMode, mDisplay.get());
            }
            else if (hw->getDisplayType() >= DisplayDevice::DISPLAY_VIRTUAL)
            {
                ALOGW("Attempt to set active color mode %s %d for virtual display",
                      decodeColorMode(mMode).c_str(), mMode);
            }
            else
            {
                mFlinger.setActiveColorModeInternal(hw, mMode, Dataspace::UNKNOWN,
                                                    RenderIntent::COLORIMETRIC);
            }
            return true;
        }
    };
    sp<MessageBase> msg = new MessageSetActiveColorMode(*this, display, colorMode);
    postMessageSync(msg);
    return NO_ERROR;
}

status_t SurfaceFlinger::clearAnimationFrameStats()
{
    Mutex::Autolock _l(mStateLock);
    mAnimFrameTracker.clearStats();
    return NO_ERROR;
}

status_t SurfaceFlinger::getAnimationFrameStats(FrameStats *outStats) const
{
    Mutex::Autolock _l(mStateLock);
    mAnimFrameTracker.getStats(outStats);
    return NO_ERROR;
}

status_t SurfaceFlinger::getHdrCapabilities(const sp<IBinder> &display,
                                            HdrCapabilities *outCapabilities) const
{
    Mutex::Autolock _l(mStateLock);

    sp<const DisplayDevice> displayDevice(getDisplayDeviceLocked(display));
    if (displayDevice == nullptr)
    {
        ALOGE("getHdrCapabilities: Invalid display %p", displayDevice.get());
        return BAD_VALUE;
    }

    // At this point the DisplayDeivce should already be set up,
    // meaning the luminance information is already queried from
    // hardware composer and stored properly.
    const HdrCapabilities &capabilities = displayDevice->getHdrCapabilities();
    *outCapabilities = HdrCapabilities(capabilities.getSupportedHdrTypes(),
                                       capabilities.getDesiredMaxLuminance(),
                                       capabilities.getDesiredMaxAverageLuminance(),
                                       capabilities.getDesiredMinLuminance());

    return NO_ERROR;
}

status_t SurfaceFlinger::enableVSyncInjections(bool enable)
{
    sp<LambdaMessage> enableVSyncInjections = new LambdaMessage([&]() {
        Mutex::Autolock _l(mStateLock);

        if (mInjectVSyncs == enable)
        {
            return;
        }

        if (enable)
        {
            ALOGV("VSync Injections enabled");
            if (mVSyncInjector.get() == nullptr)
            {
                mVSyncInjector = std::make_unique<InjectVSyncSource>();
                mInjectorEventThread = std::make_unique<
                    impl::EventThread>(
                    mVSyncInjector.get(),
                    [this]() { resyncWithRateLimit(); },
                    impl::EventThread::InterceptVSyncsCallback(),
                    "injEventThread");
            }
            mEventQueue->setEventThread(mInjectorEventThread.get());
        }
        else
        {
            ALOGV("VSync Injections disabled");
            mEventQueue->setEventThread(mSFEventThread.get());
        }

        mInjectVSyncs = enable;
    });
    postMessageSync(enableVSyncInjections);
    return NO_ERROR;
}

status_t SurfaceFlinger::injectVSync(nsecs_t when)
{
    Mutex::Autolock _l(mStateLock);

    if (!mInjectVSyncs)
    {
        ALOGE("VSync Injections not enabled");
        return BAD_VALUE;
    }
    if (mInjectVSyncs && mInjectorEventThread.get() != nullptr)
    {
        ALOGV("Injecting VSync inside SurfaceFlinger");
        mVSyncInjector->onInjectSyncEvent(when);
    }
    return NO_ERROR;
}

status_t SurfaceFlinger::getLayerDebugInfo(std::vector<LayerDebugInfo> *outLayers) const
    NO_THREAD_SAFETY_ANALYSIS
{
    IPCThreadState *ipc = IPCThreadState::self();
    const int pid = ipc->getCallingPid();
    const int uid = ipc->getCallingUid();
    if ((uid != AID_SHELL) &&
        !PermissionCache::checkPermission(sDump, pid, uid))
    {
        ALOGE("Layer debug info permission denied for pid=%d, uid=%d", pid, uid);
        return PERMISSION_DENIED;
    }

    // Try to acquire a lock for 1s, fail gracefully
    const status_t err = mStateLock.timedLock(s2ns(1));
    const bool locked = (err == NO_ERROR);
    if (!locked)
    {
        ALOGE("LayerDebugInfo: SurfaceFlinger unresponsive (%s [%d]) - exit", strerror(-err), err);
        return TIMED_OUT;
    }

    outLayers->clear();
    mCurrentState.traverseInZOrder([&](Layer *layer) {
        outLayers->push_back(layer->getLayerDebugInfo());
    });

    mStateLock.unlock();
    return NO_ERROR;
}

// ----------------------------------------------------------------------------

sp<IDisplayEventConnection> SurfaceFlinger::createDisplayEventConnection(
    ISurfaceComposer::VsyncSource vsyncSource)
{
    if (vsyncSource == eVsyncSourceSurfaceFlinger)
    {
        return mSFEventThread->createEventConnection();
    }
    else
    {
        return mEventThread->createEventConnection();
    }
}

// ----------------------------------------------------------------------------

void SurfaceFlinger::waitForEvent()
{
    mEventQueue->waitMessage();
}

void SurfaceFlinger::signalTransaction()
{
    mEventQueue->invalidate();
}

void SurfaceFlinger::signalLayerUpdate()
{
    mEventQueue->invalidate();
}

void SurfaceFlinger::signalRefresh()
{
    mRefreshPending = true;
    mEventQueue->refresh();
}

status_t SurfaceFlinger::postMessageAsync(const sp<MessageBase> &msg,
                                          nsecs_t reltime, uint32_t /* flags */)
{
    return mEventQueue->postMessage(msg, reltime);
}

status_t SurfaceFlinger::postMessageSync(const sp<MessageBase> &msg,
                                         nsecs_t reltime, uint32_t /* flags */)
{
    status_t res = mEventQueue->postMessage(msg, reltime);
    if (res == NO_ERROR)
    {
        msg->wait();
    }
    return res;
}

void SurfaceFlinger::run()
{
    do
    {
        waitForEvent();
    } while (true);
}

void SurfaceFlinger::enableHardwareVsync()
{
    Mutex::Autolock _l(mHWVsyncLock);
    if (!mPrimaryHWVsyncEnabled && mHWVsyncAvailable)
    {
        mPrimaryDispSync.beginResync();
        //eventControl(HWC_DISPLAY_PRIMARY, SurfaceFlinger::EVENT_VSYNC, true);
        mEventControlThread->setVsyncEnabled(true);
        mPrimaryHWVsyncEnabled = true;
    }
}

void SurfaceFlinger::resyncToHardwareVsync(bool makeAvailable)
{
    Mutex::Autolock _l(mHWVsyncLock);

    if (makeAvailable)
    {
        mHWVsyncAvailable = true;
    }
    else if (!mHWVsyncAvailable)
    {
        // Hardware vsync is not currently available, so abort the resync
        // attempt for now
        return;
    }

    const auto &activeConfig = getBE().mHwc->getActiveConfig(HWC_DISPLAY_PRIMARY);
    const nsecs_t period = activeConfig->getVsyncPeriod();

    mPrimaryDispSync.reset();
    mPrimaryDispSync.setPeriod(period);

    if (!mPrimaryHWVsyncEnabled)
    {
        mPrimaryDispSync.beginResync();
        //eventControl(HWC_DISPLAY_PRIMARY, SurfaceFlinger::EVENT_VSYNC, true);
        mEventControlThread->setVsyncEnabled(true);
        mPrimaryHWVsyncEnabled = true;
    }
}

void SurfaceFlinger::disableHardwareVsync(bool makeUnavailable)
{
    Mutex::Autolock _l(mHWVsyncLock);
    if (mPrimaryHWVsyncEnabled)
    {
        //eventControl(HWC_DISPLAY_PRIMARY, SurfaceFlinger::EVENT_VSYNC, false);
        mEventControlThread->setVsyncEnabled(false);
        mPrimaryDispSync.endResync();
        mPrimaryHWVsyncEnabled = false;
    }
    if (makeUnavailable)
    {
        mHWVsyncAvailable = false;
    }
}

void SurfaceFlinger::resyncWithRateLimit()
{
    static constexpr nsecs_t kIgnoreDelay = ms2ns(500);

    // No explicit locking is needed here since EventThread holds a lock while calling this method
    static nsecs_t sLastResyncAttempted = 0;
    const nsecs_t now = systemTime();
    if (now - sLastResyncAttempted > kIgnoreDelay)
    {
        resyncToHardwareVsync(false);
    }
    sLastResyncAttempted = now;
}

void SurfaceFlinger::onVsyncReceived(int32_t sequenceId,
                                     hwc2_display_t displayId, int64_t timestamp)
{
    Mutex::Autolock lock(mStateLock);
    // Ignore any vsyncs from a previous hardware composer.
    if (sequenceId != getBE().mComposerSequenceId)
    {
        return;
    }

    int32_t type;
    if (!getBE().mHwc->onVsync(displayId, timestamp, &type))
    {
        return;
    }

    bool needsHwVsync = false;

    { // Scope for the lock
        Mutex::Autolock _l(mHWVsyncLock);
        if (type == DisplayDevice::DISPLAY_PRIMARY && mPrimaryHWVsyncEnabled)
        {
            needsHwVsync = mPrimaryDispSync.addResyncSample(timestamp);
        }
    }

    if (needsHwVsync)
    {
        enableHardwareVsync();
    }
    else
    {
        disableHardwareVsync(false);
    }
}

void SurfaceFlinger::getCompositorTiming(CompositorTiming *compositorTiming)
{
    std::lock_guard<std::mutex> lock(getBE().mCompositorTimingLock);
    *compositorTiming = getBE().mCompositorTiming;
}

void SurfaceFlinger::onHotplugReceived(int32_t sequenceId, hwc2_display_t display,
                                       HWC2::Connection connection)
{
    ALOGV("onHotplugReceived(%d, %" PRIu64 ", %s)", sequenceId, display,
          connection == HWC2::Connection::Connected ? "connected" : "disconnected");

    // Ignore events that do not have the right sequenceId.
    if (sequenceId != getBE().mComposerSequenceId)
    {
        return;
    }

    // Only lock if we're not on the main thread. This function is normally
    // called on a hwbinder thread, but for the primary display it's called on
    // the main thread with the state lock already held, so don't attempt to
    // acquire it here.
    ConditionalLock lock(mStateLock, std::this_thread::get_id() != mMainThreadId);

    mPendingHotplugEvents.emplace_back(HotplugEvent{display, connection});

    if (std::this_thread::get_id() == mMainThreadId)
    {
        // Process all pending hot plug events immediately if we are on the main thread.
        processDisplayHotplugEventsLocked();
    }

    setTransactionFlags(eDisplayTransactionNeeded);
}

void SurfaceFlinger::onRefreshReceived(int sequenceId,
                                       hwc2_display_t /*display*/)
{
    Mutex::Autolock lock(mStateLock);
    if (sequenceId != getBE().mComposerSequenceId)
    {
        return;
    }
    repaintEverything();
}

void SurfaceFlinger::setVsyncEnabled(int disp, int enabled)
{
    ATRACE_CALL();
    Mutex::Autolock lock(mStateLock);
    getHwComposer().setVsyncEnabled(disp,
                                    enabled ? HWC2::Vsync::Enable : HWC2::Vsync::Disable);
}

// Note: it is assumed the caller holds |mStateLock| when this is called
void SurfaceFlinger::resetDisplayState()
{
    disableHardwareVsync(true);
    // Clear the drawing state so that the logic inside of
    // handleTransactionLocked will fire. It will determine the delta between
    // mCurrentState and mDrawingState and re-apply all changes when we make the
    // transition.
    mDrawingState.displays.clear();
    getRenderEngine().resetCurrentSurface();
    mDisplays.clear();
}

void SurfaceFlinger::updateVrFlinger()
{
    if (!mVrFlinger)
        return;
    bool vrFlingerRequestsDisplay = mVrFlingerRequestsDisplay;
    if (vrFlingerRequestsDisplay == getBE().mHwc->isUsingVrComposer())
    {
        return;
    }

    if (vrFlingerRequestsDisplay && !getBE().mHwc->getComposer()->isRemote())
    {
        ALOGE("Vr flinger is only supported for remote hardware composer"
              " service connections. Ignoring request to transition to vr"
              " flinger.");
        mVrFlingerRequestsDisplay = false;
        return;
    }

    Mutex::Autolock _l(mStateLock);

    int currentDisplayPowerMode = getDisplayDeviceLocked(
                                      mBuiltinDisplays[DisplayDevice::DISPLAY_PRIMARY])
                                      ->getPowerMode();

    if (!vrFlingerRequestsDisplay)
    {
        mVrFlinger->SeizeDisplayOwnership();
    }

    resetDisplayState();
    getBE().mHwc.reset(); // Delete the current instance before creating the new one
    getBE().mHwc.reset(new HWComposer(std::make_unique<Hwc2::impl::Composer>(
        vrFlingerRequestsDisplay ? "vr" : getBE().mHwcServiceName)));
    getBE().mHwc->registerCallback(this, ++getBE().mComposerSequenceId);

    LOG_ALWAYS_FATAL_IF(!getBE().mHwc->getComposer()->isRemote(),
                        "Switched to non-remote hardware composer");

    if (vrFlingerRequestsDisplay)
    {
        mVrFlinger->GrantDisplayOwnership();
    }
    else
    {
        enableHardwareVsync();
    }

    mVisibleRegionsDirty = true;
    invalidateHwcGeometry();

    // Re-enable default display.
    sp<DisplayDevice> hw(getDisplayDeviceLocked(
        mBuiltinDisplays[DisplayDevice::DISPLAY_PRIMARY]));
    setPowerModeInternal(hw, currentDisplayPowerMode, /*stateLockHeld*/ true);

    // Reset the timing values to account for the period of the swapped in HWC
    const auto &activeConfig = getBE().mHwc->getActiveConfig(HWC_DISPLAY_PRIMARY);
    const nsecs_t period = activeConfig->getVsyncPeriod();
    mAnimFrameTracker.setDisplayRefreshPeriod(period);

    // Use phase of 0 since phase is not known.
    // Use latency of 0, which will snap to the ideal latency.
    setCompositorTimingSnapped(0, period, 0);

    android_atomic_or(1, &mRepaintEverything);
    setTransactionFlags(eDisplayTransactionNeeded);
}

void SurfaceFlinger::onMessageReceived(int32_t what)
{
    ATRACE_CALL();
    switch (what)
    {
    case MessageQueue::INVALIDATE:
    {
        bool frameMissed = !mHadClientComposition &&
                           mPreviousPresentFence != Fence::NO_FENCE &&
                           (mPreviousPresentFence->getSignalTime() ==
                            Fence::SIGNAL_TIME_PENDING);
        ATRACE_INT("FrameMissed", static_cast<int>(frameMissed));
        if (frameMissed)
        {
            mTimeStats.incrementMissedFrames();
            if (mPropagateBackpressure)
            {
                signalLayerUpdate();
                break;
            }
        }

        // Now that we're going to make it to the handleMessageTransaction()
        // call below it's safe to call updateVrFlinger(), which will
        // potentially trigger a display handoff.
        updateVrFlinger();

        bool refreshNeeded = handleMessageTransaction();
        refreshNeeded |= handleMessageInvalidate();
        refreshNeeded |= mRepaintEverything;
        if (refreshNeeded && CC_LIKELY(mBootStage != BootStage::BOOTLOADER))
        {
            // Signal a refresh if a transaction modified the window state,
            // a new buffer was latched, or if HWC has requested a full
            // repaint
            signalRefresh();
        }
        break;
    }
    case MessageQueue::REFRESH:
    {
        handleMessageRefresh();
        break;
    }
    }
}

bool SurfaceFlinger::handleMessageTransaction()
{
    uint32_t transactionFlags = peekTransactionFlags();
    if (transactionFlags)
    {
        handleTransaction(transactionFlags);
        return true;
    }
    return false;
}

bool SurfaceFlinger::handleMessageInvalidate()
{
    ATRACE_CALL();
    return handlePageFlip();
}

void SurfaceFlinger::handleMessageRefresh()
{
    ATRACE_CALL();

    mRefreshPending = false;

    nsecs_t refreshStartTime = systemTime(SYSTEM_TIME_MONOTONIC);

    preComposition(refreshStartTime);
    rebuildLayerStacks();
    setUpHWComposer();
    doDebugFlashRegions();
    doTracing("handleRefresh");
    logLayerStats();
    doComposition();
    postComposition(refreshStartTime);

    mPreviousPresentFence = getBE().mHwc->getPresentFence(HWC_DISPLAY_PRIMARY);

    mHadClientComposition = false;
    for (size_t displayId = 0; displayId < mDisplays.size(); ++displayId)
    {
        const sp<DisplayDevice> &displayDevice = mDisplays[displayId];
        mHadClientComposition = mHadClientComposition ||
                                getBE().mHwc->hasClientComposition(displayDevice->getHwcDisplayId());
    }
    mVsyncModulator.onRefreshed(mHadClientComposition);

    mLayersWithQueuedFrames.clear();
}

void SurfaceFlinger::doDebugFlashRegions()
{
    // is debugging enabled
    if (CC_LIKELY(!mDebugRegion))
        return;

    const bool repaintEverything = mRepaintEverything;
    for (size_t dpy = 0; dpy < mDisplays.size(); dpy++)
    {
        const sp<DisplayDevice> &hw(mDisplays[dpy]);
        if (hw->isDisplayOn())
        {
            // transform the dirty region into this screen's coordinate space
            const Region dirtyRegion(hw->getDirtyRegion(repaintEverything));
            if (!dirtyRegion.isEmpty())
            {
                // redraw the whole screen
                doComposeSurfaces(hw);

                // and draw the dirty region
                const int32_t height = hw->getHeight();
                auto &engine(getRenderEngine());
                engine.fillRegionWithColor(dirtyRegion, height, 1, 0, 1, 1);

                hw->swapBuffers(getHwComposer());
            }
        }
    }

    postFramebuffer();

    if (mDebugRegion > 1)
    {
        usleep(mDebugRegion * 1000);
    }

    for (size_t displayId = 0; displayId < mDisplays.size(); ++displayId)
    {
        auto &displayDevice = mDisplays[displayId];
        if (!displayDevice->isDisplayOn())
        {
            continue;
        }

        status_t result = displayDevice->prepareFrame(*getBE().mHwc);
        ALOGE_IF(result != NO_ERROR,
                 "prepareFrame for display %zd failed:"
                 " %d (%s)",
                 displayId, result, strerror(-result));
    }
}

void SurfaceFlinger::doTracing(const char *where)
{
    ATRACE_CALL();
    ATRACE_NAME(where);
    if (CC_UNLIKELY(mTracing.isEnabled()))
    {
        mTracing.traceLayers(where, dumpProtoInfo(LayerVector::StateSet::Drawing));
    }
}

void SurfaceFlinger::logLayerStats()
{
    ATRACE_CALL();
    if (CC_UNLIKELY(mLayerStats.isEnabled()))
    {
        int32_t hwcId = -1;
        for (size_t dpy = 0; dpy < mDisplays.size(); ++dpy)
        {
            const sp<const DisplayDevice> &displayDevice(mDisplays[dpy]);
            if (displayDevice->isPrimary())
            {
                hwcId = displayDevice->getHwcDisplayId();
                break;
            }
        }
        if (hwcId < 0)
        {
            ALOGE("LayerStats: Hmmm, no primary display?");
            return;
        }
        mLayerStats.logLayerStats(dumpVisibleLayersProtoInfo(hwcId));
    }
}

void SurfaceFlinger::preComposition(nsecs_t refreshStartTime)
{
    ATRACE_CALL();
    ALOGV("preComposition");

    bool needExtraInvalidate = false;
    mDrawingState.traverseInZOrder([&](Layer *layer) {
        if (layer->onPreComposition(refreshStartTime))
        {
            needExtraInvalidate = true;
        }
    });

    if (needExtraInvalidate)
    {
        signalLayerUpdate();
    }
}

void SurfaceFlinger::updateCompositorTiming(
    nsecs_t vsyncPhase, nsecs_t vsyncInterval, nsecs_t compositeTime,
    std::shared_ptr<FenceTime> &presentFenceTime)
{
    // Update queue of past composite+present times and determine the
    // most recently known composite to present latency.
    getBE().mCompositePresentTimes.push({compositeTime, presentFenceTime});
    nsecs_t compositeToPresentLatency = -1;
    while (!getBE().mCompositePresentTimes.empty())
    {
        SurfaceFlingerBE::CompositePresentTime &cpt = getBE().mCompositePresentTimes.front();
        // Cached values should have been updated before calling this method,
        // which helps avoid duplicate syscalls.
        nsecs_t displayTime = cpt.display->getCachedSignalTime();
        if (displayTime == Fence::SIGNAL_TIME_PENDING)
        {
            break;
        }
        compositeToPresentLatency = displayTime - cpt.composite;
        getBE().mCompositePresentTimes.pop();
    }

    // Don't let mCompositePresentTimes grow unbounded, just in case.
    while (getBE().mCompositePresentTimes.size() > 16)
    {
        getBE().mCompositePresentTimes.pop();
    }

    setCompositorTimingSnapped(
        vsyncPhase, vsyncInterval, compositeToPresentLatency);
}

void SurfaceFlinger::setCompositorTimingSnapped(nsecs_t vsyncPhase,
                                                nsecs_t vsyncInterval, nsecs_t compositeToPresentLatency)
{
    // Integer division and modulo round toward 0 not -inf, so we need to
    // treat negative and positive offsets differently.
    nsecs_t idealLatency = (sfVsyncPhaseOffsetNs > 0) ? (vsyncInterval - (sfVsyncPhaseOffsetNs % vsyncInterval)) : ((-sfVsyncPhaseOffsetNs) % vsyncInterval);

    // Just in case sfVsyncPhaseOffsetNs == -vsyncInterval.
    if (idealLatency <= 0)
    {
        idealLatency = vsyncInterval;
    }

    // Snap the latency to a value that removes scheduling jitter from the
    // composition and present times, which often have >1ms of jitter.
    // Reducing jitter is important if an app attempts to extrapolate
    // something (such as user input) to an accurate diasplay time.
    // Snapping also allows an app to precisely calculate sfVsyncPhaseOffsetNs
    // with (presentLatency % interval).
    nsecs_t bias = vsyncInterval / 2;
    int64_t extraVsyncs =
        (compositeToPresentLatency - idealLatency + bias) / vsyncInterval;
    nsecs_t snappedCompositeToPresentLatency = (extraVsyncs > 0) ? idealLatency + (extraVsyncs * vsyncInterval) : idealLatency;

    std::lock_guard<std::mutex> lock(getBE().mCompositorTimingLock);
    getBE().mCompositorTiming.deadline = vsyncPhase - idealLatency;
    getBE().mCompositorTiming.interval = vsyncInterval;
    getBE().mCompositorTiming.presentLatency = snappedCompositeToPresentLatency;
}

void SurfaceFlinger::postComposition(nsecs_t refreshStartTime)
{
    ATRACE_CALL();
    ALOGV("postComposition");

    // Release any buffers which were replaced this frame
    nsecs_t dequeueReadyTime = systemTime();
    for (auto &layer : mLayersWithQueuedFrames)
    {
        layer->releasePendingBuffer(dequeueReadyTime);
    }

    // |mStateLock| not needed as we are on the main thread
    const sp<const DisplayDevice> hw(getDefaultDisplayDeviceLocked());

    getBE().mGlCompositionDoneTimeline.updateSignalTimes();
    std::shared_ptr<FenceTime> glCompositionDoneFenceTime;
    if (hw && getBE().mHwc->hasClientComposition(HWC_DISPLAY_PRIMARY))
    {
        glCompositionDoneFenceTime =
            std::make_shared<FenceTime>(hw->getClientTargetAcquireFence());
        getBE().mGlCompositionDoneTimeline.push(glCompositionDoneFenceTime);
    }
    else
    {
        glCompositionDoneFenceTime = FenceTime::NO_FENCE;
    }

    getBE().mDisplayTimeline.updateSignalTimes();
    sp<Fence> presentFence = getBE().mHwc->getPresentFence(HWC_DISPLAY_PRIMARY);
    auto presentFenceTime = std::make_shared<FenceTime>(presentFence);
    getBE().mDisplayTimeline.push(presentFenceTime);

    nsecs_t vsyncPhase = mPrimaryDispSync.computeNextRefresh(0);
    nsecs_t vsyncInterval = mPrimaryDispSync.getPeriod();

    // We use the refreshStartTime which might be sampled a little later than
    // when we started doing work for this frame, but that should be okay
    // since updateCompositorTiming has snapping logic.
    updateCompositorTiming(
        vsyncPhase, vsyncInterval, refreshStartTime, presentFenceTime);
    CompositorTiming compositorTiming;
    {
        std::lock_guard<std::mutex> lock(getBE().mCompositorTimingLock);
        compositorTiming = getBE().mCompositorTiming;
    }

    mDrawingState.traverseInZOrder([&](Layer *layer) {
        bool frameLatched = layer->onPostComposition(glCompositionDoneFenceTime,
                                                     presentFenceTime, compositorTiming);
        if (frameLatched)
        {
            recordBufferingStats(layer->getName().string(),
                                 layer->getOccupancyHistory(false));
        }
    });

    if (presentFenceTime->isValid())
    {
        if (mPrimaryDispSync.addPresentFence(presentFenceTime))
        {
            enableHardwareVsync();
        }
        else
        {
            disableHardwareVsync(false);
        }
    }

    if (!hasSyncFramework)
    {
        if (getBE().mHwc->isConnected(HWC_DISPLAY_PRIMARY) && hw->isDisplayOn())
        {
            enableHardwareVsync();
        }
    }

    if (mAnimCompositionPending)
    {
        mAnimCompositionPending = false;

        if (presentFenceTime->isValid())
        {
            mAnimFrameTracker.setActualPresentFence(
                std::move(presentFenceTime));
        }
        else if (getBE().mHwc->isConnected(HWC_DISPLAY_PRIMARY))
        {
            // The HWC doesn't support present fences, so use the refresh
            // timestamp instead.
            nsecs_t presentTime =
                getBE().mHwc->getRefreshTimestamp(HWC_DISPLAY_PRIMARY);
            mAnimFrameTracker.setActualPresentTime(presentTime);
        }
        mAnimFrameTracker.advanceFrame();
    }

    mTimeStats.incrementTotalFrames();
    if (mHadClientComposition)
    {
        mTimeStats.incrementClientCompositionFrames();
    }

    if (getBE().mHwc->isConnected(HWC_DISPLAY_PRIMARY) &&
        hw->getPowerMode() == HWC_POWER_MODE_OFF)
    {
        return;
    }

    nsecs_t currentTime = systemTime();
    if (mHasPoweredOff)
    {
        mHasPoweredOff = false;
    }
    else
    {
        nsecs_t elapsedTime = currentTime - getBE().mLastSwapTime;
        size_t numPeriods = static_cast<size_t>(elapsedTime / vsyncInterval);
        if (numPeriods < SurfaceFlingerBE::NUM_BUCKETS - 1)
        {
            getBE().mFrameBuckets[numPeriods] += elapsedTime;
        }
        else
        {
            getBE().mFrameBuckets[SurfaceFlingerBE::NUM_BUCKETS - 1] += elapsedTime;
        }
        getBE().mTotalTime += elapsedTime;
    }
    getBE().mLastSwapTime = currentTime;

    {
        std::lock_guard lock(mTexturePoolMutex);
        const size_t refillCount = mTexturePoolSize - mTexturePool.size();
        if (refillCount > 0)
        {
            const size_t offset = mTexturePool.size();
            mTexturePool.resize(mTexturePoolSize);
            getRenderEngine().genTextures(refillCount, mTexturePool.data() + offset);
            ATRACE_INT("TexturePoolSize", mTexturePool.size());
        }
    }
}

void SurfaceFlinger::rebuildLayerStacks()
{
    ATRACE_CALL();
    ALOGV("rebuildLayerStacks");

    // rebuild the visible layer list per screen
    if (CC_UNLIKELY(mVisibleRegionsDirty))
    {
        ATRACE_NAME("rebuildLayerStacks VR Dirty");
        mVisibleRegionsDirty = false;
        invalidateHwcGeometry();

        for (size_t dpy = 0; dpy < mDisplays.size(); dpy++)
        {
            Region opaqueRegion;
            Region dirtyRegion;
            Vector<sp<Layer>> layersSortedByZ;
            Vector<sp<Layer>> layersNeedingFences;
            const sp<DisplayDevice> &displayDevice(mDisplays[dpy]);
            const Transform &tr(displayDevice->getTransform());
            const Rect bounds(displayDevice->getBounds());
            if (displayDevice->isDisplayOn())
            {
                computeVisibleRegions(displayDevice, dirtyRegion, opaqueRegion);

                mDrawingState.traverseInZOrder([&](Layer *layer) {
                    bool hwcLayerDestroyed = false;
                    if (layer->belongsToDisplay(displayDevice->getLayerStack(),
                                                displayDevice->isPrimary()))
                    {
                        Region drawRegion(tr.transform(
                            layer->visibleNonTransparentRegion));
                        drawRegion.andSelf(bounds);
                        if (!drawRegion.isEmpty())
                        {
                            layersSortedByZ.add(layer);
                        }
                        else
                        {
                            // Clear out the HWC layer if this layer was
                            // previously visible, but no longer is
                            hwcLayerDestroyed = layer->destroyHwcLayer(
                                displayDevice->getHwcDisplayId());
                        }
                    }
                    else
                    {
                        // WM changes displayDevice->layerStack upon sleep/awake.
                        // Here we make sure we delete the HWC layers even if
                        // WM changed their layer stack.
                        hwcLayerDestroyed = layer->destroyHwcLayer(
                            displayDevice->getHwcDisplayId());
                    }

                    // If a layer is not going to get a release fence because
                    // it is invisible, but it is also going to release its
                    // old buffer, add it to the list of layers needing
                    // fences.
                    if (hwcLayerDestroyed)
                    {
                        auto found = std::find(mLayersWithQueuedFrames.cbegin(),
                                               mLayersWithQueuedFrames.cend(), layer);
                        if (found != mLayersWithQueuedFrames.cend())
                        {
                            layersNeedingFences.add(layer);
                        }
                    }
                });
            }
            displayDevice->setVisibleLayersSortedByZ(layersSortedByZ);
            displayDevice->setLayersNeedingFences(layersNeedingFences);
            displayDevice->undefinedRegion.set(bounds);
            displayDevice->undefinedRegion.subtractSelf(
                tr.transform(opaqueRegion));
            displayDevice->dirtyRegion.orSelf(dirtyRegion);
        }
    }
}

// Returns a data space that fits all visible layers.  The returned data space
// can only be one of
//  - Dataspace::SRGB (use legacy dataspace and let HWC saturate when colors are enhanced)
//  - Dataspace::DISPLAY_P3
// The returned HDR data space is one of
//  - Dataspace::UNKNOWN
//  - Dataspace::BT2020_HLG
//  - Dataspace::BT2020_PQ
Dataspace SurfaceFlinger::getBestDataspace(
    const sp<const DisplayDevice> &displayDevice, Dataspace *outHdrDataSpace) const
{
    Dataspace bestDataSpace = Dataspace::SRGB;
    *outHdrDataSpace = Dataspace::UNKNOWN;

    for (const auto &layer : displayDevice->getVisibleLayersSortedByZ())
    {
        switch (layer->getDataSpace())
        {
        case Dataspace::V0_SCRGB:
        case Dataspace::V0_SCRGB_LINEAR:
        case Dataspace::DISPLAY_P3:
            bestDataSpace = Dataspace::DISPLAY_P3;
            break;
        case Dataspace::BT2020_PQ:
        case Dataspace::BT2020_ITU_PQ:
            *outHdrDataSpace = Dataspace::BT2020_PQ;
            break;
        case Dataspace::BT2020_HLG:
        case Dataspace::BT2020_ITU_HLG:
            // When there's mixed PQ content and HLG content, we set the HDR
            // data space to be BT2020_PQ and convert HLG to PQ.
            if (*outHdrDataSpace == Dataspace::UNKNOWN)
            {
                *outHdrDataSpace = Dataspace::BT2020_HLG;
            }
            break;
        default:
            break;
        }
    }

    return bestDataSpace;
}

// Pick the ColorMode / Dataspace for the display device.
void SurfaceFlinger::pickColorMode(const sp<DisplayDevice> &displayDevice,
                                   ColorMode *outMode, Dataspace *outDataSpace,
                                   RenderIntent *outRenderIntent) const
{
    if (mDisplayColorSetting == DisplayColorSetting::UNMANAGED)
    {
        *outMode = ColorMode::NATIVE;
        *outDataSpace = Dataspace::UNKNOWN;
        *outRenderIntent = RenderIntent::COLORIMETRIC;
        return;
    }

    Dataspace hdrDataSpace;
    Dataspace bestDataSpace = getBestDataspace(displayDevice, &hdrDataSpace);

    // respect hdrDataSpace only when there is no legacy HDR support
    const bool isHdr = hdrDataSpace != Dataspace::UNKNOWN &&
                       !displayDevice->hasLegacyHdrSupport(hdrDataSpace);
    if (isHdr)
    {
        bestDataSpace = hdrDataSpace;
    }

    RenderIntent intent;
    switch (mDisplayColorSetting)
    {
    case DisplayColorSetting::MANAGED:
    case DisplayColorSetting::UNMANAGED:
        intent = isHdr ? RenderIntent::TONE_MAP_COLORIMETRIC : RenderIntent::COLORIMETRIC;
        break;
    case DisplayColorSetting::ENHANCED:
        intent = isHdr ? RenderIntent::TONE_MAP_ENHANCE : RenderIntent::ENHANCE;
        break;
    default: // vendor display color setting
        intent = static_cast<RenderIntent>(mDisplayColorSetting);
        break;
    }

    displayDevice->getBestColorMode(bestDataSpace, intent, outDataSpace, outMode, outRenderIntent);
}

void SurfaceFlinger::setUpHWComposer()
{
    ATRACE_CALL();
    ALOGV("setUpHWComposer");

    for (size_t dpy = 0; dpy < mDisplays.size(); dpy++)
    {
        bool dirty = !mDisplays[dpy]->getDirtyRegion(mRepaintEverything).isEmpty();
        bool empty = mDisplays[dpy]->getVisibleLayersSortedByZ().size() == 0;
        bool wasEmpty = !mDisplays[dpy]->lastCompositionHadVisibleLayers;

        // If nothing has changed (!dirty), don't recompose.
        // If something changed, but we don't currently have any visible layers,
        //   and didn't when we last did a composition, then skip it this time.
        // The second rule does two things:
        // - When all layers are removed from a display, we'll emit one black
        //   frame, then nothing more until we get new layers.
        // - When a display is created with a private layer stack, we won't
        //   emit any black frames until a layer is added to the layer stack.
        bool mustRecompose = dirty && !(empty && wasEmpty);

        ALOGV_IF(mDisplays[dpy]->getDisplayType() == DisplayDevice::DISPLAY_VIRTUAL,
                 "dpy[%zu]: %s composition (%sdirty %sempty %swasEmpty)", dpy,
                 mustRecompose ? "doing" : "skipping",
                 dirty ? "+" : "-",
                 empty ? "+" : "-",
                 wasEmpty ? "+" : "-");

        mDisplays[dpy]->beginFrame(mustRecompose);

        if (mustRecompose)
        {
            mDisplays[dpy]->lastCompositionHadVisibleLayers = !empty;
        }
    }

    // build the h/w work list
    if (CC_UNLIKELY(mGeometryInvalid))
    {
        mGeometryInvalid = false;
        for (size_t dpy = 0; dpy < mDisplays.size(); dpy++)
        {
            sp<const DisplayDevice> displayDevice(mDisplays[dpy]);
            const auto hwcId = displayDevice->getHwcDisplayId();
            if (hwcId >= 0)
            {
                const Vector<sp<Layer>> &currentLayers(
                    displayDevice->getVisibleLayersSortedByZ());
                for (size_t i = 0; i < currentLayers.size(); i++)
                {
                    const auto &layer = currentLayers[i];
                    if (!layer->hasHwcLayer(hwcId))
                    {
                        if (!layer->createHwcLayer(getBE().mHwc.get(), hwcId))
                        {
                            layer->forceClientComposition(hwcId);
                            continue;
                        }
                    }

                    layer->setGeometry(displayDevice, i);
                    if (mDebugDisableHWC || mDebugRegion)
                    {
                        layer->forceClientComposition(hwcId);
                    }
                }
            }
        }
    }

    // Set the per-frame data
    for (size_t displayId = 0; displayId < mDisplays.size(); ++displayId)
    {
        auto &displayDevice = mDisplays[displayId];
        const auto hwcId = displayDevice->getHwcDisplayId();

        if (hwcId < 0)
        {
            continue;
        }
        if (mDrawingState.colorMatrixChanged)
        {
            displayDevice->setColorTransform(mDrawingState.colorMatrix);
            status_t result = getBE().mHwc->setColorTransform(hwcId, mDrawingState.colorMatrix);
            ALOGE_IF(result != NO_ERROR, "Failed to set color transform on "
                                         "display %zd: %d",
                     displayId, result);
        }
        for (auto &layer : displayDevice->getVisibleLayersSortedByZ())
        {
            if (layer->isHdrY410())
            {
                layer->forceClientComposition(hwcId);
            }
            else if ((layer->getDataSpace() == Dataspace::BT2020_PQ ||
                      layer->getDataSpace() == Dataspace::BT2020_ITU_PQ) &&
                     !displayDevice->hasHDR10Support())
            {
                layer->forceClientComposition(hwcId);
            }
            else if ((layer->getDataSpace() == Dataspace::BT2020_HLG ||
                      layer->getDataSpace() == Dataspace::BT2020_ITU_HLG) &&
                     !displayDevice->hasHLGSupport())
            {
                layer->forceClientComposition(hwcId);
            }

            if (layer->getForceClientComposition(hwcId))
            {
                ALOGV("[%s] Requesting Client composition", layer->getName().string());
                layer->setCompositionType(hwcId, HWC2::Composition::Client);
                continue;
            }

            layer->setPerFrameData(displayDevice);
        }

        if (hasWideColorDisplay)
        {
            ColorMode colorMode;
            Dataspace dataSpace;
            RenderIntent renderIntent;
            pickColorMode(displayDevice, &colorMode, &dataSpace, &renderIntent);
            setActiveColorModeInternal(displayDevice, colorMode, dataSpace, renderIntent);
        }
    }

    mDrawingState.colorMatrixChanged = false;

    for (size_t displayId = 0; displayId < mDisplays.size(); ++displayId)
    {
        auto &displayDevice = mDisplays[displayId];
        if (!displayDevice->isDisplayOn())
        {
            continue;
        }

        status_t result = displayDevice->prepareFrame(*getBE().mHwc);
        ALOGE_IF(result != NO_ERROR, "prepareFrame for display %zd failed:"
                                     " %d (%s)",
                 displayId, result, strerror(-result));
    }
}

void SurfaceFlinger::doComposition()
{
    ATRACE_CALL();
    ALOGV("doComposition");

    const bool repaintEverything = android_atomic_and(0, &mRepaintEverything);
    for (size_t dpy = 0; dpy < mDisplays.size(); dpy++)
    {
        const sp<DisplayDevice> &hw(mDisplays[dpy]);
        if (hw->isDisplayOn())
        {
            // transform the dirty region into this screen's coordinate space
            const Region dirtyRegion(hw->getDirtyRegion(repaintEverything));

            // repaint the framebuffer (if needed)
            doDisplayComposition(hw, dirtyRegion);

            hw->dirtyRegion.clear();
            hw->flip();
        }
    }
    postFramebuffer();
}

void SurfaceFlinger::postFramebuffer()
{
    ATRACE_CALL();
    ALOGV("postFramebuffer");

    const nsecs_t now = systemTime();
    mDebugInSwapBuffers = now;

    for (size_t displayId = 0; displayId < mDisplays.size(); ++displayId)
    {
        auto &displayDevice = mDisplays[displayId];
        if (!displayDevice->isDisplayOn())
        {
            continue;
        }
        const auto hwcId = displayDevice->getHwcDisplayId();
        if (hwcId >= 0)
        {
            getBE().mHwc->presentAndGetReleaseFences(hwcId);
        }
        displayDevice->onSwapBuffersCompleted();
        displayDevice->makeCurrent();
        for (auto &layer : displayDevice->getVisibleLayersSortedByZ())
        {
            sp<Fence> releaseFence = Fence::NO_FENCE;

            // The layer buffer from the previous frame (if any) is released
            // by HWC only when the release fence from this frame (if any) is
            // signaled.  Always get the release fence from HWC first.
            auto hwcLayer = layer->getHwcLayer(hwcId);
            if (hwcId >= 0)
            {
                releaseFence = getBE().mHwc->getLayerReleaseFence(hwcId, hwcLayer);
            }

            // If the layer was client composited in the previous frame, we
            // need to merge with the previous client target acquire fence.
            // Since we do not track that, always merge with the current
            // client target acquire fence when it is available, even though
            // this is suboptimal.
            if (layer->getCompositionType(hwcId) == HWC2::Composition::Client)
            {
                releaseFence = Fence::merge("LayerRelease", releaseFence,
                                            displayDevice->getClientTargetAcquireFence());
            }

            layer->onLayerDisplayed(releaseFence);
        }

        // We've got a list of layers needing fences, that are disjoint with
        // displayDevice->getVisibleLayersSortedByZ.  The best we can do is to
        // supply them with the present fence.
        if (!displayDevice->getLayersNeedingFences().isEmpty())
        {
            sp<Fence> presentFence = getBE().mHwc->getPresentFence(hwcId);
            for (auto &layer : displayDevice->getLayersNeedingFences())
            {
                layer->onLayerDisplayed(presentFence);
            }
        }

        if (hwcId >= 0)
        {
            getBE().mHwc->clearReleaseFences(hwcId);
        }
    }

    mLastSwapBufferTime = systemTime() - now;
    mDebugInSwapBuffers = 0;

    // |mStateLock| not needed as we are on the main thread
    if (getBE().mHwc->isConnected(HWC_DISPLAY_PRIMARY))
    {
        uint32_t flipCount = getDefaultDisplayDeviceLocked()->getPageFlipCount();
        if (flipCount % LOG_FRAME_STATS_PERIOD == 0)
        {
            logFrameStats();
        }
    }
}

void SurfaceFlinger::handleTransaction(uint32_t transactionFlags)
{
    ATRACE_CALL();

    // here we keep a copy of the drawing state (that is the state that's
    // going to be overwritten by handleTransactionLocked()) outside of
    // mStateLock so that the side-effects of the State assignment
    // don't happen with mStateLock held (which can cause deadlocks).
    State drawingState(mDrawingState);

    Mutex::Autolock _l(mStateLock);
    const nsecs_t now = systemTime();
    mDebugInTransaction = now;

    // Here we're guaranteed that some transaction flags are set
    // so we can call handleTransactionLocked() unconditionally.
    // We call getTransactionFlags(), which will also clear the flags,
    // with mStateLock held to guarantee that mCurrentState won't change
    // until the transaction is committed.

    mVsyncModulator.onTransactionHandled();
    transactionFlags = getTransactionFlags(eTransactionMask);
    handleTransactionLocked(transactionFlags);

    mLastTransactionTime = systemTime() - now;
    mDebugInTransaction = 0;
    invalidateHwcGeometry();
    // here the transaction has been committed
}

DisplayDevice::DisplayType SurfaceFlinger::determineDisplayType(hwc2_display_t display,
                                                                HWC2::Connection connection) const
{
    // Figure out whether the event is for the primary display or an
    // external display by matching the Hwc display id against one for a
    // connected display. If we did not find a match, we then check what
    // displays are not already connected to determine the type. If we don't
    // have a connected primary display, we assume the new display is meant to
    // be the primary display, and then if we don't have an external display,
    // we assume it is that.
    const auto primaryDisplayId =
        getBE().mHwc->getHwcDisplayId(DisplayDevice::DISPLAY_PRIMARY);
    const auto externalDisplayId =
        getBE().mHwc->getHwcDisplayId(DisplayDevice::DISPLAY_EXTERNAL);
    if (primaryDisplayId && primaryDisplayId == display)
    {
        return DisplayDevice::DISPLAY_PRIMARY;
    }
    else if (externalDisplayId && externalDisplayId == display)
    {
        return DisplayDevice::DISPLAY_EXTERNAL;
    }
    else if (connection == HWC2::Connection::Connected && !primaryDisplayId)
    {
        return DisplayDevice::DISPLAY_PRIMARY;
    }
    else if (connection == HWC2::Connection::Connected && !externalDisplayId)
    {
        return DisplayDevice::DISPLAY_EXTERNAL;
    }

    return DisplayDevice::DISPLAY_ID_INVALID;
}

void SurfaceFlinger::processDisplayHotplugEventsLocked()
{
    for (const auto &event : mPendingHotplugEvents)
    {
        auto displayType = determineDisplayType(event.display, event.connection);
        if (displayType == DisplayDevice::DISPLAY_ID_INVALID)
        {
            ALOGW("Unable to determine the display type for display %" PRIu64, event.display);
            continue;
        }

        if (getBE().mHwc->isUsingVrComposer() && displayType == DisplayDevice::DISPLAY_EXTERNAL)
        {
            ALOGE("External displays are not supported by the vr hardware composer.");
            continue;
        }

        getBE().mHwc->onHotplug(event.display, displayType, event.connection);

        if (event.connection == HWC2::Connection::Connected)
        {
            if (!mBuiltinDisplays[displayType].get())
            {
                ALOGV("Creating built in display %d", displayType);
                mBuiltinDisplays[displayType] = new BBinder();
                // All non-virtual displays are currently considered secure.
                DisplayDeviceState info(displayType, true);
                info.displayName = displayType == DisplayDevice::DISPLAY_PRIMARY ? "Built-in Screen" : "External Screen";
                mCurrentState.displays.add(mBuiltinDisplays[displayType], info);
                mInterceptor->saveDisplayCreation(info);
            }
        }
        else
        {
            ALOGV("Removing built in display %d", displayType);

            ssize_t idx = mCurrentState.displays.indexOfKey(mBuiltinDisplays[displayType]);
            if (idx >= 0)
            {
                const DisplayDeviceState &info(mCurrentState.displays.valueAt(idx));
                mInterceptor->saveDisplayDeletion(info.displayId);
                mCurrentState.displays.removeItemsAt(idx);
            }
            mBuiltinDisplays[displayType].clear();
        }

        processDisplayChangesLocked();
    }

    mPendingHotplugEvents.clear();
}

sp<DisplayDevice> SurfaceFlinger::setupNewDisplayDeviceInternal(
    const wp<IBinder> &display, int hwcId, const DisplayDeviceState &state,
    const sp<DisplaySurface> &dispSurface, const sp<IGraphicBufferProducer> &producer)
{
    bool hasWideColorGamut = false;
    std::unordered_map<ColorMode, std::vector<RenderIntent>> hwcColorModes;
    HdrCapabilities hdrCapabilities;
    int32_t supportedPerFrameMetadata = 0;

    if (hasWideColorDisplay && hwcId >= 0)
    {
        std::vector<ColorMode> modes = getHwComposer().getColorModes(hwcId);
        for (ColorMode colorMode : modes)
        {
            switch (colorMode)
            {
            case ColorMode::DISPLAY_P3:
            case ColorMode::ADOBE_RGB:
            case ColorMode::DCI_P3:
                hasWideColorGamut = true;
                break;
            default:
                break;
            }

            std::vector<RenderIntent> renderIntents = getHwComposer().getRenderIntents(hwcId,
                                                                                       colorMode);
            hwcColorModes.emplace(colorMode, renderIntents);
        }
    }

    if (hwcId >= 0)
    {
        getHwComposer().getHdrCapabilities(hwcId, &hdrCapabilities);
        supportedPerFrameMetadata = getHwComposer().getSupportedPerFrameMetadata(hwcId);
    }

    auto nativeWindowSurface = mCreateNativeWindowSurface(producer);
    auto nativeWindow = nativeWindowSurface->getNativeWindow();

    /*
     * Create our display's surface
     */
    std::unique_ptr<RE::Surface> renderSurface = getRenderEngine().createSurface();
    renderSurface->setCritical(state.type == DisplayDevice::DISPLAY_PRIMARY);
    renderSurface->setAsync(state.type >= DisplayDevice::DISPLAY_VIRTUAL);
    renderSurface->setNativeWindow(nativeWindow.get());
    const int displayWidth = renderSurface->queryWidth();
    const int displayHeight = renderSurface->queryHeight();

    // Make sure that composition can never be stalled by a virtual display
    // consumer that isn't processing buffers fast enough. We have to do this
    // in two places:
    // * Here, in case the display is composed entirely by HWC.
    // * In makeCurrent(), using eglSwapInterval. Some EGL drivers set the
    //   window's swap interval in eglMakeCurrent, so they'll override the
    //   interval we set here.
    if (state.type >= DisplayDevice::DISPLAY_VIRTUAL)
    {
        nativeWindow->setSwapInterval(nativeWindow.get(), 0);
    }

    // virtual displays are always considered enabled
    auto initialPowerMode = (state.type >= DisplayDevice::DISPLAY_VIRTUAL) ? HWC_POWER_MODE_NORMAL
                                                                           : HWC_POWER_MODE_OFF;

    sp<DisplayDevice> hw =
        new DisplayDevice(this, state.type, hwcId, state.isSecure, display, nativeWindow,
                          dispSurface, std::move(renderSurface), displayWidth, displayHeight,
                          hasWideColorGamut, hdrCapabilities,
                          supportedPerFrameMetadata, hwcColorModes, initialPowerMode);

    if (maxFrameBufferAcquiredBuffers >= 3)
    {
        nativeWindowSurface->preallocateBuffers();
    }

    ColorMode defaultColorMode = ColorMode::NATIVE;
    Dataspace defaultDataSpace = Dataspace::UNKNOWN;
    if (hasWideColorGamut)
    {
        defaultColorMode = ColorMode::SRGB;
        defaultDataSpace = Dataspace::SRGB;
    }
    setActiveColorModeInternal(hw, defaultColorMode, defaultDataSpace,
                               RenderIntent::COLORIMETRIC);
    if (state.type < DisplayDevice::DISPLAY_VIRTUAL)
    {
        hw->setActiveConfig(getHwComposer().getActiveConfigIndex(state.type));
    }
    hw->setLayerStack(state.layerStack);
    hw->setProjection(state.orientation, state.viewport, state.frame);
    hw->setDisplayName(state.displayName);

    return hw;
}

void SurfaceFlinger::processDisplayChangesLocked()
{
    // here we take advantage of Vector's copy-on-write semantics to
    // improve performance by skipping the transaction entirely when
    // know that the lists are identical
    const KeyedVector<wp<IBinder>, DisplayDeviceState> &curr(mCurrentState.displays);
    const KeyedVector<wp<IBinder>, DisplayDeviceState> &draw(mDrawingState.displays);
    if (!curr.isIdenticalTo(draw))
    {
        mVisibleRegionsDirty = true;
        const size_t cc = curr.size();
        size_t dc = draw.size();

        // find the displays that were removed
        // (ie: in drawing state but not in current state)
        // also handle displays that changed
        // (ie: displays that are in both lists)
        for (size_t i = 0; i < dc;)
        {
            const ssize_t j = curr.indexOfKey(draw.keyAt(i));
            if (j < 0)
            {
                // in drawing state but not in current state
                // Call makeCurrent() on the primary display so we can
                // be sure that nothing associated with this display
                // is current.
                const sp<const DisplayDevice> defaultDisplay(getDefaultDisplayDeviceLocked());
                if (defaultDisplay != nullptr)
                    defaultDisplay->makeCurrent();
                sp<DisplayDevice> hw(getDisplayDeviceLocked(draw.keyAt(i)));
                if (hw != nullptr)
                    hw->disconnect(getHwComposer());
                if (draw[i].type < DisplayDevice::NUM_BUILTIN_DISPLAY_TYPES)
                    mEventThread->onHotplugReceived(draw[i].type, false);
                mDisplays.removeItem(draw.keyAt(i));
            }
            else
            {
                // this display is in both lists. see if something changed.
                const DisplayDeviceState &state(curr[j]);
                const wp<IBinder> &display(curr.keyAt(j));
                const sp<IBinder> state_binder = IInterface::asBinder(state.surface);
                const sp<IBinder> draw_binder = IInterface::asBinder(draw[i].surface);
                if (state_binder != draw_binder)
                {
                    // changing the surface is like destroying and
                    // recreating the DisplayDevice, so we just remove it
                    // from the drawing state, so that it get re-added
                    // below.
                    sp<DisplayDevice> hw(getDisplayDeviceLocked(display));
                    if (hw != nullptr)
                        hw->disconnect(getHwComposer());
                    mDisplays.removeItem(display);
                    mDrawingState.displays.removeItemsAt(i);
                    dc--;
                    // at this point we must loop to the next item
                    continue;
                }

                const sp<DisplayDevice> disp(getDisplayDeviceLocked(display));
                if (disp != nullptr)
                {
                    if (state.layerStack != draw[i].layerStack)
                    {
                        disp->setLayerStack(state.layerStack);
                    }
                    if ((state.orientation != draw[i].orientation) ||
                        (state.viewport != draw[i].viewport) || (state.frame != draw[i].frame))
                    {
                        disp->setProjection(state.orientation, state.viewport, state.frame);
                    }
                    if (state.width != draw[i].width || state.height != draw[i].height)
                    {
                        disp->setDisplaySize(state.width, state.height);
                    }
                }
            }
            ++i;
        }

        // find displays that were added
        // (ie: in current state but not in drawing state)
        for (size_t i = 0; i < cc; i++)
        {
            if (draw.indexOfKey(curr.keyAt(i)) < 0)
            {
                const DisplayDeviceState &state(curr[i]);

                sp<DisplaySurface> dispSurface;
                sp<IGraphicBufferProducer> producer;
                sp<IGraphicBufferProducer> bqProducer;
                sp<IGraphicBufferConsumer> bqConsumer;
                mCreateBufferQueue(&bqProducer, &bqConsumer, false);

                int32_t hwcId = -1;
                if (state.isVirtualDisplay())
                {
                    // Virtual displays without a surface are dormant:
                    // they have external state (layer stack, projection,
                    // etc.) but no internal state (i.e. a DisplayDevice).
                    if (state.surface != nullptr)
                    {
                        // Allow VR composer to use virtual displays.
                        if (mUseHwcVirtualDisplays || getBE().mHwc->isUsingVrComposer())
                        {
                            int width = 0;
                            int status = state.surface->query(NATIVE_WINDOW_WIDTH, &width);
                            ALOGE_IF(status != NO_ERROR, "Unable to query width (%d)", status);
                            int height = 0;
                            status = state.surface->query(NATIVE_WINDOW_HEIGHT, &height);
                            ALOGE_IF(status != NO_ERROR, "Unable to query height (%d)", status);
                            int intFormat = 0;
                            status = state.surface->query(NATIVE_WINDOW_FORMAT, &intFormat);
                            ALOGE_IF(status != NO_ERROR, "Unable to query format (%d)", status);
                            auto format = static_cast<ui::PixelFormat>(intFormat);

                            getBE().mHwc->allocateVirtualDisplay(width, height, &format, &hwcId);
                        }

                        // TODO: Plumb requested format back up to consumer

                        sp<VirtualDisplaySurface> vds =
                            new VirtualDisplaySurface(*getBE().mHwc, hwcId, state.surface,
                                                      bqProducer, bqConsumer,
                                                      state.displayName);

                        dispSurface = vds;
                        producer = vds;
                    }
                }
                else
                {
                    ALOGE_IF(state.surface != nullptr,
                             "adding a supported display, but rendering "
                             "surface is provided (%p), ignoring it",
                             state.surface.get());

                    hwcId = state.type;
                    dispSurface = new FramebufferSurface(*getBE().mHwc, hwcId, bqConsumer);
                    producer = bqProducer;
                }

                const wp<IBinder> &display(curr.keyAt(i));
                if (dispSurface != nullptr)
                {
                    mDisplays.add(display,
                                  setupNewDisplayDeviceInternal(display, hwcId, state, dispSurface,
                                                                producer));
                    if (!state.isVirtualDisplay())
                    {
                        mEventThread->onHotplugReceived(state.type, true);
                    }
                }
            }
        }
    }

    mDrawingState.displays = mCurrentState.displays;
}

void SurfaceFlinger::handleTransactionLocked(uint32_t transactionFlags)
{
    // Notify all layers of available frames
    mCurrentState.traverseInZOrder([](Layer *layer) {
        layer->notifyAvailableFrames();
    });

    /*
     * Traversal of the children
     * (perform the transaction for each of them if needed)
     */

    if (transactionFlags & eTraversalNeeded)
    {
        mCurrentState.traverseInZOrder([&](Layer *layer) {
            uint32_t trFlags = layer->getTransactionFlags(eTransactionNeeded);
            if (!trFlags)
                return;

            const uint32_t flags = layer->doTransaction(0);
            if (flags & Layer::eVisibleRegion)
                mVisibleRegionsDirty = true;
        });
    }

    /*
     * Perform display own transactions if needed
     */

    if (transactionFlags & eDisplayTransactionNeeded)
    {
        processDisplayChangesLocked();
        processDisplayHotplugEventsLocked();
    }

    if (transactionFlags & (eDisplayLayerStackChanged | eDisplayTransactionNeeded))
    {
        // The transform hint might have changed for some layers
        // (either because a display has changed, or because a layer
        // as changed).
        //
        // Walk through all the layers in currentLayers,
        // and update their transform hint.
        //
        // If a layer is visible only on a single display, then that
        // display is used to calculate the hint, otherwise we use the
        // default display.
        //
        // NOTE: we do this here, rather than in rebuildLayerStacks() so that
        // the hint is set before we acquire a buffer from the surface texture.
        //
        // NOTE: layer transactions have taken place already, so we use their
        // drawing state. However, SurfaceFlinger's own transaction has not
        // happened yet, so we must use the current state layer list
        // (soon to become the drawing state list).
        //
        sp<const DisplayDevice> disp;
        uint32_t currentlayerStack = 0;
        bool first = true;
        mCurrentState.traverseInZOrder([&](Layer *layer) {
            // NOTE: we rely on the fact that layers are sorted by
            // layerStack first (so we don't have to traverse the list
            // of displays for every layer).
            uint32_t layerStack = layer->getLayerStack();
            if (first || currentlayerStack != layerStack)
            {
                currentlayerStack = layerStack;
                // figure out if this layerstack is mirrored
                // (more than one display) if so, pick the default display,
                // if not, pick the only display it's on.
                disp.clear();
                for (size_t dpy = 0; dpy < mDisplays.size(); dpy++)
                {
                    sp<const DisplayDevice> hw(mDisplays[dpy]);
                    if (layer->belongsToDisplay(hw->getLayerStack(), hw->isPrimary()))
                    {
                        if (disp == nullptr)
                        {
                            disp = std::move(hw);
                        }
                        else
                        {
                            disp = nullptr;
                            break;
                        }
                    }
                }
            }

            if (disp == nullptr)
            {
                // NOTE: TEMPORARY FIX ONLY. Real fix should cause layers to
                // redraw after transform hint changes. See bug 8508397.

                // could be null when this layer is using a layerStack
                // that is not visible on any display. Also can occur at
                // screen off/on times.
                disp = getDefaultDisplayDeviceLocked();
            }

            // disp can be null if there is no display available at all to get
            // the transform hint from.
            if (disp != nullptr)
            {
                layer->updateTransformHint(disp);
            }

            first = false;
        });
    }

    /*
     * Perform our own transaction if needed
     */

    if (mLayersAdded)
    {
        mLayersAdded = false;
        // Layers have been added.
        mVisibleRegionsDirty = true;
    }

    // some layers might have been removed, so
    // we need to update the regions they're exposing.
    if (mLayersRemoved)
    {
        mLayersRemoved = false;
        mVisibleRegionsDirty = true;
        mDrawingState.traverseInZOrder([&](Layer *layer) {
            if (mLayersPendingRemoval.indexOf(layer) >= 0)
            {
                // this layer is not visible anymore
                // TODO: we could traverse the tree from front to back and
                //       compute the actual visible region
                // TODO: we could cache the transformed region
                Region visibleReg;
                visibleReg.set(layer->computeScreenBounds());
                invalidateLayerStack(layer, visibleReg);
            }
        });
    }

    commitTransaction();

    updateCursorAsync();
}

void SurfaceFlinger::updateCursorAsync()
{
    for (size_t displayId = 0; displayId < mDisplays.size(); ++displayId)
    {
        auto &displayDevice = mDisplays[displayId];
        if (displayDevice->getHwcDisplayId() < 0)
        {
            continue;
        }

        for (auto &layer : displayDevice->getVisibleLayersSortedByZ())
        {
            layer->updateCursorPosition(displayDevice);
        }
    }
}

void SurfaceFlinger::commitTransaction()
{
    if (!mLayersPendingRemoval.isEmpty())
    {
        // Notify removed layers now that they can't be drawn from
        for (const auto &l : mLayersPendingRemoval)
        {
            recordBufferingStats(l->getName().string(),
                                 l->getOccupancyHistory(true));
            l->onRemoved();
        }
        mLayersPendingRemoval.clear();
    }

    // If this transaction is part of a window animation then the next frame
    // we composite should be considered an animation as well.
    mAnimCompositionPending = mAnimTransactionPending;

    mDrawingState = mCurrentState;
    // clear the "changed" flags in current state
    mCurrentState.colorMatrixChanged = false;

    mDrawingState.traverseInZOrder([](Layer *layer) {
        layer->commitChildList();
    });
    mTransactionPending = false;
    mAnimTransactionPending = false;
    mTransactionCV.broadcast();
}

void SurfaceFlinger::computeVisibleRegions(const sp<const DisplayDevice> &displayDevice,
                                           Region &outDirtyRegion, Region &outOpaqueRegion)
{
    ATRACE_CALL();
    ALOGV("computeVisibleRegions");

    Region aboveOpaqueLayers;
    Region aboveCoveredLayers;
    Region dirty;

    outDirtyRegion.clear();

    mDrawingState.traverseInReverseZOrder([&](Layer *layer) {
        // start with the whole surface at its current location
        const Layer::State &s(layer->getDrawingState());

        // only consider the layers on the given layer stack
        if (!layer->belongsToDisplay(displayDevice->getLayerStack(), displayDevice->isPrimary()))
            return;

        /*
         * opaqueRegion: area of a surface that is fully opaque.
         */
        Region opaqueRegion;

        /*
         * visibleRegion: area of a surface that is visible on screen
         * and not fully transparent. This is essentially the layer's
         * footprint minus the opaque regions above it.
         * Areas covered by a translucent surface are considered visible.
         */
        Region visibleRegion;

        /*
         * coveredRegion: area of a surface that is covered by all
         * visible regions above it (which includes the translucent areas).
         */
        Region coveredRegion;

        /*
         * transparentRegion: area of a surface that is hinted to be completely
         * transparent. This is only used to tell when the layer has no visible
         * non-transparent regions and can be removed from the layer list. It
         * does not affect the visibleRegion of this layer or any layers
         * beneath it. The hint may not be correct if apps don't respect the
         * SurfaceView restrictions (which, sadly, some don't).
         */
        Region transparentRegion;

        // handle hidden surfaces by setting the visible region to empty
        if (CC_LIKELY(layer->isVisible()))
        {
            const bool translucent = !layer->isOpaque(s);
            Rect bounds(layer->computeScreenBounds());
            visibleRegion.set(bounds);
            Transform tr = layer->getTransform();
            if (!visibleRegion.isEmpty())
            {
                // Remove the transparent area from the visible region
                if (translucent)
                {
                    if (tr.preserveRects())
                    {
                        // transform the transparent region
                        transparentRegion = tr.transform(s.activeTransparentRegion);
                    }
                    else
                    {
                        // transformation too complex, can't do the
                        // transparent region optimization.
                        transparentRegion.clear();
                    }
                }

                // compute the opaque region
                const int32_t layerOrientation = tr.getOrientation();
                if (layer->getAlpha() == 1.0f && !translucent &&
                    ((layerOrientation & Transform::ROT_INVALID) == false))
                {
                    // the opaque region is the layer's footprint
                    opaqueRegion = visibleRegion;
                }
            }
        }

        if (visibleRegion.isEmpty())
        {
            layer->clearVisibilityRegions();
            return;
        }

        // Clip the covered region to the visible region
        coveredRegion = aboveCoveredLayers.intersect(visibleRegion);

        // Update aboveCoveredLayers for next (lower) layer
        aboveCoveredLayers.orSelf(visibleRegion);

        // subtract the opaque region covered by the layers above us
        visibleRegion.subtractSelf(aboveOpaqueLayers);

        // compute this layer's dirty region
        if (layer->contentDirty)
        {
            // we need to invalidate the whole region
            dirty = visibleRegion;
            // as well, as the old visible region
            dirty.orSelf(layer->visibleRegion);
            layer->contentDirty = false;
        }
        else
        {
            /* compute the exposed region:
             *   the exposed region consists of two components:
             *   1) what's VISIBLE now and was COVERED before
             *   2) what's EXPOSED now less what was EXPOSED before
             *
             * note that (1) is conservative, we start with the whole
             * visible region but only keep what used to be covered by
             * something -- which mean it may have been exposed.
             *
             * (2) handles areas that were not covered by anything but got
             * exposed because of a resize.
             */
            const Region newExposed = visibleRegion - coveredRegion;
            const Region oldVisibleRegion = layer->visibleRegion;
            const Region oldCoveredRegion = layer->coveredRegion;
            const Region oldExposed = oldVisibleRegion - oldCoveredRegion;
            dirty = (visibleRegion & oldCoveredRegion) | (newExposed - oldExposed);
        }
        dirty.subtractSelf(aboveOpaqueLayers);

        // accumulate to the screen dirty region
        outDirtyRegion.orSelf(dirty);

        // Update aboveOpaqueLayers for next (lower) layer
        aboveOpaqueLayers.orSelf(opaqueRegion);

        // Store the visible region in screen space
        layer->setVisibleRegion(visibleRegion);
        layer->setCoveredRegion(coveredRegion);
        layer->setVisibleNonTransparentRegion(
            visibleRegion.subtract(transparentRegion));
    });

    outOpaqueRegion = aboveOpaqueLayers;
}

void SurfaceFlinger::invalidateLayerStack(const sp<const Layer> &layer, const Region &dirty)
{
    for (size_t dpy = 0; dpy < mDisplays.size(); dpy++)
    {
        const sp<DisplayDevice> &hw(mDisplays[dpy]);
        if (layer->belongsToDisplay(hw->getLayerStack(), hw->isPrimary()))
        {
            hw->dirtyRegion.orSelf(dirty);
        }
    }
}

bool SurfaceFlinger::handlePageFlip()
{
    ALOGV("handlePageFlip");

    nsecs_t latchTime = systemTime();

    bool visibleRegions = false;
    bool frameQueued = false;
    bool newDataLatched = false;

    // Store the set of layers that need updates. This set must not change as
    // buffers are being latched, as this could result in a deadlock.
    // Example: Two producers share the same command stream and:
    // 1.) Layer 0 is latched
    // 2.) Layer 0 gets a new frame
    // 2.) Layer 1 gets a new frame
    // 3.) Layer 1 is latched.
    // Display is now waiting on Layer 1's frame, which is behind layer 0's
    // second frame. But layer 0's second frame could be waiting on display.
    mDrawingState.traverseInZOrder([&](Layer *layer) {
        if (layer->hasQueuedFrame())
        {
            frameQueued = true;
            if (layer->shouldPresentNow(mPrimaryDispSync))
            {
                mLayersWithQueuedFrames.push_back(layer);
            }
            else
            {
                layer->useEmptyDamage();
            }
        }
        else
        {
            layer->useEmptyDamage();
        }
    });

    for (auto &layer : mLayersWithQueuedFrames)
    {
        const Region dirty(layer->latchBuffer(visibleRegions, latchTime));
        layer->useSurfaceDamage();
        invalidateLayerStack(layer, dirty);
        if (layer->isBufferLatched())
        {
            newDataLatched = true;
        }
    }

    mVisibleRegionsDirty |= visibleRegions;

    // If we will need to wake up at some time in the future to deal with a
    // queued frame that shouldn't be displayed during this vsync period, wake
    // up during the next vsync period to check again.
    if (frameQueued && (mLayersWithQueuedFrames.empty() || !newDataLatched))
    {
        signalLayerUpdate();
    }

    // enter boot animation on first buffer latch
    if (CC_UNLIKELY(mBootStage == BootStage::BOOTLOADER && newDataLatched))
    {
        ALOGI("Enter boot animation");
        mBootStage = BootStage::BOOTANIMATION;
    }

    // Only continue with the refresh if there is actually new work to do
    return !mLayersWithQueuedFrames.empty() && newDataLatched;
}

void SurfaceFlinger::invalidateHwcGeometry()
{
    mGeometryInvalid = true;
}

void SurfaceFlinger::doDisplayComposition(
    const sp<const DisplayDevice> &displayDevice,
    const Region &inDirtyRegion)
{
    // We only need to actually compose the display if:
    // 1) It is being handled by hardware composer, which may need this to
    //    keep its virtual display state machine in sync, or
    // 2) There is work to be done (the dirty region isn't empty)
    bool isHwcDisplay = displayDevice->getHwcDisplayId() >= 0;
    if (!isHwcDisplay && inDirtyRegion.isEmpty())
    {
        ALOGV("Skipping display composition");
        return;
    }

    ALOGV("doDisplayComposition");
    if (!doComposeSurfaces(displayDevice))
        return;

    // swap buffers (presentation)
    displayDevice->swapBuffers(getHwComposer());
}

bool SurfaceFlinger::doComposeSurfaces(const sp<const DisplayDevice> &displayDevice)
{
    ALOGV("doComposeSurfaces");

    const Region bounds(displayDevice->bounds());
    const DisplayRenderArea renderArea(displayDevice);
    const auto hwcId = displayDevice->getHwcDisplayId();
    const bool hasClientComposition = getBE().mHwc->hasClientComposition(hwcId);
    ATRACE_INT("hasClientComposition", hasClientComposition);

    bool applyColorMatrix = false;
    bool needsEnhancedColorMatrix = false;

    if (hasClientComposition)
    {
        ALOGV("hasClientComposition");

        Dataspace outputDataspace = Dataspace::UNKNOWN;
        if (displayDevice->hasWideColorGamut())
        {
            outputDataspace = displayDevice->getCompositionDataSpace();
        }
        getBE().mRenderEngine->setOutputDataSpace(outputDataspace);
        getBE().mRenderEngine->setDisplayMaxLuminance(
            displayDevice->getHdrCapabilities().getDesiredMaxLuminance());

        const bool hasDeviceComposition = getBE().mHwc->hasDeviceComposition(hwcId);
        const bool skipClientColorTransform = getBE().mHwc->hasCapability(
            HWC2::Capability::SkipClientColorTransform);

        mat4 colorMatrix;
        applyColorMatrix = !hasDeviceComposition && !skipClientColorTransform;
        if (applyColorMatrix)
        {
            colorMatrix = mDrawingState.colorMatrix;
        }

        // The current enhanced saturation matrix is designed to enhance Display P3,
        // thus we only apply this matrix when the render intent is not colorimetric
        // and the output color space is Display P3.
        needsEnhancedColorMatrix =
            (displayDevice->getActiveRenderIntent() >= RenderIntent::ENHANCE &&
             outputDataspace == Dataspace::DISPLAY_P3);
        if (needsEnhancedColorMatrix)
        {
            colorMatrix *= mEnhancedSaturationMatrix;
        }

        getRenderEngine().setupColorTransform(colorMatrix);

        if (!displayDevice->makeCurrent())
        {
            ALOGW("DisplayDevice::makeCurrent failed. Aborting surface composition for display %s",
                  displayDevice->getDisplayName().string());
            getRenderEngine().resetCurrentSurface();

            // |mStateLock| not needed as we are on the main thread
            if (!getDefaultDisplayDeviceLocked()->makeCurrent())
            {
                ALOGE("DisplayDevice::makeCurrent on default display failed. Aborting.");
            }
            return false;
        }

        // Never touch the framebuffer if we don't have any framebuffer layers
        if (hasDeviceComposition)
        {
            // when using overlays, we assume a fully transparent framebuffer
            // NOTE: we could reduce how much we need to clear, for instance
            // remove where there are opaque FB layers. however, on some
            // GPUs doing a "clean slate" clear might be more efficient.
            // We'll revisit later if needed.
            getBE().mRenderEngine->clearWithColor(0, 0, 0, 0);
        }
        else
        {
            // we start with the whole screen area and remove the scissor part
            // we're left with the letterbox region
            // (common case is that letterbox ends-up being empty)
            const Region letterbox(bounds.subtract(displayDevice->getScissor()));

            // compute the area to clear
            Region region(displayDevice->undefinedRegion.merge(letterbox));

            // screen is already cleared here
            if (!region.isEmpty())
            {
                // can happen with SurfaceView
                drawWormhole(displayDevice, region);
            }
        }

        const Rect &bounds(displayDevice->getBounds());
        const Rect &scissor(displayDevice->getScissor());
        if (scissor != bounds)
        {
            // scissor doesn't match the screen's dimensions, so we
            // need to clear everything outside of it and enable
            // the GL scissor so we don't draw anything where we shouldn't

            // enable scissor for this frame
            const uint32_t height = displayDevice->getHeight();
            getBE().mRenderEngine->setScissor(scissor.left, height - scissor.bottom,
                                              scissor.getWidth(), scissor.getHeight());
        }
    }

    /*
     * and then, render the layers targeted at the framebuffer
     */

    ALOGV("Rendering client layers");
    const Transform &displayTransform = displayDevice->getTransform();
    bool firstLayer = true;
    for (auto &layer : displayDevice->getVisibleLayersSortedByZ())
    {
        const Region clip(bounds.intersect(
            displayTransform.transform(layer->visibleRegion)));
        ALOGV("Layer: %s", layer->getName().string());
        ALOGV("  Composition type: %s",
              to_string(layer->getCompositionType(hwcId)).c_str());
        if (!clip.isEmpty())
        {
            switch (layer->getCompositionType(hwcId))
            {
            case HWC2::Composition::Cursor:
            case HWC2::Composition::Device:
            case HWC2::Composition::Sideband:
            case HWC2::Composition::SolidColor:
            {
                const Layer::State &state(layer->getDrawingState());
                if (layer->getClearClientTarget(hwcId) && !firstLayer &&
                    layer->isOpaque(state) && (state.color.a == 1.0f) && hasClientComposition)
                {
                    // never clear the very first layer since we're
                    // guaranteed the FB is already cleared
                    layer->clearWithOpenGL(renderArea);
                }
                break;
            }
            case HWC2::Composition::Client:
            {
                layer->draw(renderArea, clip);
                break;
            }
            default:
                break;
            }
        }
        else
        {
            ALOGV("  Skipping for empty clip");
        }
        firstLayer = false;
    }

    if (applyColorMatrix || needsEnhancedColorMatrix)
    {
        getRenderEngine().setupColorTransform(mat4());
    }

    // disable scissor at the end of the frame
    getBE().mRenderEngine->disableScissor();
    return true;
}

void SurfaceFlinger::drawWormhole(const sp<const DisplayDevice> &displayDevice, const Region &region) const
{
    const int32_t height = displayDevice->getHeight();
    auto &engine(getRenderEngine());
    engine.fillRegionWithColor(region, height, 0, 0, 0, 0);
}

status_t SurfaceFlinger::addClientLayer(const sp<Client> &client,
                                        const sp<IBinder> &handle,
                                        const sp<IGraphicBufferProducer> &gbc,
                                        const sp<Layer> &lbc,
                                        const sp<Layer> &parent)
{
    // add this layer to the current state list
    {
        Mutex::Autolock _l(mStateLock);
        if (mNumLayers >= MAX_LAYERS)
        {
            ALOGE("AddClientLayer failed, mNumLayers (%zu) >= MAX_LAYERS (%zu)", mNumLayers,
                  MAX_LAYERS);
            return NO_MEMORY;
        }
        if (parent == nullptr)
        {
            mCurrentState.layersSortedByZ.add(lbc);
        }
        else
        {
            if (parent->isPendingRemoval())
            {
                ALOGE("addClientLayer called with a removed parent");
                return NAME_NOT_FOUND;
            }
            parent->addChild(lbc);
        }

        if (gbc != nullptr)
        {
            mGraphicBufferProducerList.insert(IInterface::asBinder(gbc).get());
            LOG_ALWAYS_FATAL_IF(mGraphicBufferProducerList.size() >
                                    mMaxGraphicBufferProducerListSize,
                                "Suspected IGBP leak: %zu IGBPs (%zu max), %zu Layers",
                                mGraphicBufferProducerList.size(),
                                mMaxGraphicBufferProducerListSize, mNumLayers);
        }
        mLayersAdded = true;
        mNumLayers++;
    }

    // attach this layer to the client
    client->attachLayer(handle, lbc);

    return NO_ERROR;
}

status_t SurfaceFlinger::removeLayer(const sp<Layer> &layer, bool topLevelOnly)
{
    Mutex::Autolock _l(mStateLock);
    return removeLayerLocked(mStateLock, layer, topLevelOnly);
}

status_t SurfaceFlinger::removeLayerLocked(const Mutex &, const sp<Layer> &layer,
                                           bool topLevelOnly)
{
    if (layer->isPendingRemoval())
    {
        return NO_ERROR;
    }

    const auto &p = layer->getParent();
    ssize_t index;
    if (p != nullptr)
    {
        if (topLevelOnly)
        {
            return NO_ERROR;
        }

        sp<Layer> ancestor = p;
        while (ancestor->getParent() != nullptr)
        {
            ancestor = ancestor->getParent();
        }
        if (mCurrentState.layersSortedByZ.indexOf(ancestor) < 0)
        {
            ALOGE("removeLayer called with a layer whose parent has been removed");
            return NAME_NOT_FOUND;
        }

        index = p->removeChild(layer);
    }
    else
    {
        index = mCurrentState.layersSortedByZ.remove(layer);
    }

    // As a matter of normal operation, the LayerCleaner will produce a second
    // attempt to remove the surface. The Layer will be kept alive in mDrawingState
    // so we will succeed in promoting it, but it's already been removed
    // from mCurrentState. As long as we can find it in mDrawingState we have no problem
    // otherwise something has gone wrong and we are leaking the layer.
    if (index < 0 && mDrawingState.layersSortedByZ.indexOf(layer) < 0)
    {
        ALOGE("Failed to find layer (%s) in layer parent (%s).",
              layer->getName().string(),
              (p != nullptr) ? p->getName().string() : "no-parent");
        return BAD_VALUE;
    }
    else if (index < 0)
    {
        return NO_ERROR;
    }

    layer->onRemovedFromCurrentState();
    mLayersPendingRemoval.add(layer);
    mLayersRemoved = true;
    mNumLayers -= 1 + layer->getChildrenCount();
    setTransactionFlags(eTransactionNeeded);
    return NO_ERROR;
}

uint32_t SurfaceFlinger::peekTransactionFlags()
{
    return android_atomic_release_load(&mTransactionFlags);
}

uint32_t SurfaceFlinger::getTransactionFlags(uint32_t flags)
{
    return android_atomic_and(~flags, &mTransactionFlags) & flags;
}

uint32_t SurfaceFlinger::setTransactionFlags(uint32_t flags)
{
    return setTransactionFlags(flags, VSyncModulator::TransactionStart::NORMAL);
}

uint32_t SurfaceFlinger::setTransactionFlags(uint32_t flags,
                                             VSyncModulator::TransactionStart transactionStart)
{
    uint32_t old = android_atomic_or(flags, &mTransactionFlags);
    mVsyncModulator.setTransactionStart(transactionStart);
    if ((old & flags) == 0)
    { // wake the server up
        signalTransaction();
    }
    return old;
}

bool SurfaceFlinger::containsAnyInvalidClientState(const Vector<ComposerState> &states)
{
    for (const ComposerState &state : states)
    {
        // Here we need to check that the interface we're given is indeed
        // one of our own. A malicious client could give us a nullptr
        // IInterface, or one of its own or even one of our own but a
        // different type. All these situations would cause us to crash.
        if (state.client == nullptr)
        {
            return true;
        }

        sp<IBinder> binder = IInterface::asBinder(state.client);
        if (binder == nullptr)
        {
            return true;
        }

        if (binder->queryLocalInterface(ISurfaceComposerClient::descriptor) == nullptr)
        {
            return true;
        }
    }
    return false;
}

void SurfaceFlinger::setTransactionState(
    const Vector<ComposerState> &states,
    const Vector<DisplayState> &displays,
    uint32_t flags)
{
    ATRACE_CALL();
    Mutex::Autolock _l(mStateLock);
    uint32_t transactionFlags = 0;

    if (containsAnyInvalidClientState(states))
    {
        return;
    }

    if (flags & eAnimation)
    {
        // For window updates that are part of an animation we must wait for
        // previous animation "frames" to be handled.
        while (mAnimTransactionPending)
        {
            status_t err = mTransactionCV.waitRelative(mStateLock, s2ns(5));
            if (CC_UNLIKELY(err != NO_ERROR))
            {
                // just in case something goes wrong in SF, return to the
                // caller after a few seconds.
                ALOGW_IF(err == TIMED_OUT, "setTransactionState timed out "
                                           "waiting for previous animation frame");
                mAnimTransactionPending = false;
                break;
            }
        }
    }

    for (const DisplayState &display : displays)
    {
        transactionFlags |= setDisplayStateLocked(display);
    }

    for (const ComposerState &state : states)
    {
        transactionFlags |= setClientStateLocked(state);
    }

    // Iterate through all layers again to determine if any need to be destroyed. Marking layers
    // as destroyed should only occur after setting all other states. This is to allow for a
    // child re-parent to happen before marking its original parent as destroyed (which would
    // then mark the child as destroyed).
    for (const ComposerState &state : states)
    {
        setDestroyStateLocked(state);
    }

    // If a synchronous transaction is explicitly requested without any changes, force a transaction
    // anyway. This can be used as a flush mechanism for previous async transactions.
    // Empty animation transaction can be used to simulate back-pressure, so also force a
    // transaction for empty animation transactions.
    if (transactionFlags == 0 &&
        ((flags & eSynchronous) || (flags & eAnimation)))
    {
        transactionFlags = eTransactionNeeded;
    }

    if (transactionFlags)
    {
        if (mInterceptor->isEnabled())
        {
            mInterceptor->saveTransaction(states, mCurrentState.displays, displays, flags);
        }

        // this triggers the transaction
        const auto start = (flags & eEarlyWakeup)
                               ? VSyncModulator::TransactionStart::EARLY
                               : VSyncModulator::TransactionStart::NORMAL;
        setTransactionFlags(transactionFlags, start);

        // if this is a synchronous transaction, wait for it to take effect
        // before returning.
        if (flags & eSynchronous)
        {
            mTransactionPending = true;
        }
        if (flags & eAnimation)
        {
            mAnimTransactionPending = true;
        }
        while (mTransactionPending)
        {
            status_t err = mTransactionCV.waitRelative(mStateLock, s2ns(5));
            if (CC_UNLIKELY(err != NO_ERROR))
            {
                // just in case something goes wrong in SF, return to the
                // called after a few seconds.
                ALOGW_IF(err == TIMED_OUT, "setTransactionState timed out!");
                mTransactionPending = false;
                break;
            }
        }
    }
}

uint32_t SurfaceFlinger::setDisplayStateLocked(const DisplayState &s)
{
    ssize_t dpyIdx = mCurrentState.displays.indexOfKey(s.token);
    if (dpyIdx < 0)
        return 0;

    uint32_t flags = 0;
    DisplayDeviceState &disp(mCurrentState.displays.editValueAt(dpyIdx));
    if (disp.isValid())
    {
        const uint32_t what = s.what;
        if (what & DisplayState::eSurfaceChanged)
        {
            if (IInterface::asBinder(disp.surface) != IInterface::asBinder(s.surface))
            {
                disp.surface = s.surface;
                flags |= eDisplayTransactionNeeded;
            }
        }
        if (what & DisplayState::eLayerStackChanged)
        {
            if (disp.layerStack != s.layerStack)
            {
                disp.layerStack = s.layerStack;
                flags |= eDisplayTransactionNeeded;
            }
        }
        if (what & DisplayState::eDisplayProjectionChanged)
        {
            if (disp.orientation != s.orientation)
            {
                disp.orientation = s.orientation;
                flags |= eDisplayTransactionNeeded;
            }
            if (disp.frame != s.frame)
            {
                disp.frame = s.frame;
                flags |= eDisplayTransactionNeeded;
            }
            if (disp.viewport != s.viewport)
            {
                disp.viewport = s.viewport;
                flags |= eDisplayTransactionNeeded;
            }
        }
        if (what & DisplayState::eDisplaySizeChanged)
        {
            if (disp.width != s.width)
            {
                disp.width = s.width;
                flags |= eDisplayTransactionNeeded;
            }
            if (disp.height != s.height)
            {
                disp.height = s.height;
                flags |= eDisplayTransactionNeeded;
            }
        }
    }
    return flags;
}

bool callingThreadHasUnscopedSurfaceFlingerAccess()
{
    IPCThreadState *ipc = IPCThreadState::self();
    const int pid = ipc->getCallingPid();
    const int uid = ipc->getCallingUid();

    if ((uid != AID_GRAPHICS && uid != AID_SYSTEM) &&
        !PermissionCache::checkPermission(sAccessSurfaceFlinger, pid, uid))
    {
        return false;
    }
    return true;
}

uint32_t SurfaceFlinger::setClientStateLocked(const ComposerState &composerState)
{
    const layer_state_t &s = composerState.state;
    sp<Client> client(static_cast<Client *>(composerState.client.get()));

    sp<Layer> layer(client->getLayerUser(s.surface));
    if (layer == nullptr)
    {
        return 0;
    }

    if (layer->isPendingRemoval())
    {
        ALOGW("Attempting to set client state on removed layer: %s", layer->getName().string());
        return 0;
    }

    uint32_t flags = 0;

    const uint32_t what = s.what;
    bool geometryAppliesWithResize =
        what & layer_state_t::eGeometryAppliesWithResize;

    // If we are deferring transaction, make sure to push the pending state, as otherwise the
    // pending state will also be deferred.
    if (what & layer_state_t::eDeferTransaction)
    {
        layer->pushPendingState();
    }

    if (what & layer_state_t::ePositionChanged)
    {
        if (layer->setPosition(s.x, s.y, !geometryAppliesWithResize))
        {
            flags |= eTraversalNeeded;
        }
    }
    if (what & layer_state_t::eLayerChanged)
    {
        // NOTE: index needs to be calculated before we update the state
        const auto &p = layer->getParent();
        if (p == nullptr)
        {
            ssize_t idx = mCurrentState.layersSortedByZ.indexOf(layer);
            if (layer->setLayer(s.z) && idx >= 0)
            {
                mCurrentState.layersSortedByZ.removeAt(idx);
                mCurrentState.layersSortedByZ.add(layer);
                // we need traversal (state changed)
                // AND transaction (list changed)
                flags |= eTransactionNeeded | eTraversalNeeded;
            }
        }
        else
        {
            if (p->setChildLayer(layer, s.z))
            {
                flags |= eTransactionNeeded | eTraversalNeeded;
            }
        }
    }
    if (what & layer_state_t::eRelativeLayerChanged)
    {
        // NOTE: index needs to be calculated before we update the state
        const auto &p = layer->getParent();
        if (p == nullptr)
        {
            ssize_t idx = mCurrentState.layersSortedByZ.indexOf(layer);
            if (layer->setRelativeLayer(s.relativeLayerHandle, s.z) && idx >= 0)
            {
                mCurrentState.layersSortedByZ.removeAt(idx);
                mCurrentState.layersSortedByZ.add(layer);
                // we need traversal (state changed)
                // AND transaction (list changed)
                flags |= eTransactionNeeded | eTraversalNeeded;
            }
        }
        else
        {
            if (p->setChildRelativeLayer(layer, s.relativeLayerHandle, s.z))
            {
                flags |= eTransactionNeeded | eTraversalNeeded;
            }
        }
    }
    if (what & layer_state_t::eSizeChanged)
    {
        if (layer->setSize(s.w, s.h))
        {
            flags |= eTraversalNeeded;
        }
    }
    if (what & layer_state_t::eAlphaChanged)
    {
        if (layer->setAlpha(s.alpha))
            flags |= eTraversalNeeded;
    }
    if (what & layer_state_t::eColorChanged)
    {
        if (layer->setColor(s.color))
            flags |= eTraversalNeeded;
    }
    if (what & layer_state_t::eMatrixChanged)
    {
        // TODO: b/109894387
        //
        // SurfaceFlinger's renderer is not prepared to handle cropping in the face of arbitrary
        // rotation. To see the problem observe that if we have a square parent, and a child
        // of the same size, then we rotate the child 45 degrees around it's center, the child
        // must now be cropped to a non rectangular 8 sided region.
        //
        // Of course we can fix this in the future. For now, we are lucky, SurfaceControl is
        // private API, and the WindowManager only uses rotation in one case, which is on a top
        // level layer in which cropping is not an issue.
        //
        // However given that abuse of rotation matrices could lead to surfaces extending outside
        // of cropped areas, we need to prevent non-root clients without permission ACCESS_SURFACE_FLINGER
        // (a.k.a. everyone except WindowManager and tests) from setting non rectangle preserving
        // transformations.
        if (layer->setMatrix(s.matrix, callingThreadHasUnscopedSurfaceFlingerAccess()))
            flags |= eTraversalNeeded;
    }
    if (what & layer_state_t::eTransparentRegionChanged)
    {
        if (layer->setTransparentRegionHint(s.transparentRegion))
            flags |= eTraversalNeeded;
    }
    if (what & layer_state_t::eFlagsChanged)
    {
        if (layer->setFlags(s.flags, s.mask))
            flags |= eTraversalNeeded;
    }
    if (what & layer_state_t::eCropChanged)
    {
        if (layer->setCrop(s.crop, !geometryAppliesWithResize))
            flags |= eTraversalNeeded;
    }
    if (what & layer_state_t::eFinalCropChanged)
    {
        if (layer->setFinalCrop(s.finalCrop, !geometryAppliesWithResize))
            flags |= eTraversalNeeded;
    }
    if (what & layer_state_t::eLayerStackChanged)
    {
        ssize_t idx = mCurrentState.layersSortedByZ.indexOf(layer);
        // We only allow setting layer stacks for top level layers,
        // everything else inherits layer stack from its parent.
        if (layer->hasParent())
        {
            ALOGE("Attempt to set layer stack on layer with parent (%s) is invalid",
                  layer->getName().string());
        }
        else if (idx < 0)
        {
            ALOGE("Attempt to set layer stack on layer without parent (%s) that "
                  "that also does not appear in the top level layer list. Something"
                  " has gone wrong.",
                  layer->getName().string());
        }
        else if (layer->setLayerStack(s.layerStack))
        {
            mCurrentState.layersSortedByZ.removeAt(idx);
            mCurrentState.layersSortedByZ.add(layer);
            // we need traversal (state changed)
            // AND transaction (list changed)
            flags |= eTransactionNeeded | eTraversalNeeded | eDisplayLayerStackChanged;
        }
    }
    if (what & layer_state_t::eDeferTransaction)
    {
        if (s.barrierHandle != nullptr)
        {
            layer->deferTransactionUntil(s.barrierHandle, s.frameNumber);
        }
        else if (s.barrierGbp != nullptr)
        {
            const sp<IGraphicBufferProducer> &gbp = s.barrierGbp;
            if (authenticateSurfaceTextureLocked(gbp))
            {
                const auto &otherLayer =
                    (static_cast<MonitoredProducer *>(gbp.get()))->getLayer();
                layer->deferTransactionUntil(otherLayer, s.frameNumber);
            }
            else
            {
                ALOGE("Attempt to defer transaction to to an"
                      " unrecognized GraphicBufferProducer");
            }
        }
        // We don't trigger a traversal here because if no other state is
        // changed, we don't want this to cause any more work
    }
    if (what & layer_state_t::eReparent)
    {
        bool hadParent = layer->hasParent();
        if (layer->reparent(s.parentHandleForChild))
        {
            if (!hadParent)
            {
                mCurrentState.layersSortedByZ.remove(layer);
            }
            flags |= eTransactionNeeded | eTraversalNeeded;
        }
    }
    if (what & layer_state_t::eReparentChildren)
    {
        if (layer->reparentChildren(s.reparentHandle))
        {
            flags |= eTransactionNeeded | eTraversalNeeded;
        }
    }
    if (what & layer_state_t::eDetachChildren)
    {
        layer->detachChildren();
    }
    if (what & layer_state_t::eOverrideScalingModeChanged)
    {
        layer->setOverrideScalingMode(s.overrideScalingMode);
        // We don't trigger a traversal here because if no other state is
        // changed, we don't want this to cause any more work
    }
    return flags;
}

void SurfaceFlinger::setDestroyStateLocked(const ComposerState &composerState)
{
    const layer_state_t &state = composerState.state;
    sp<Client> client(static_cast<Client *>(composerState.client.get()));

    sp<Layer> layer(client->getLayerUser(state.surface));
    if (layer == nullptr)
    {
        return;
    }

    if (layer->isPendingRemoval())
    {
        ALOGW("Attempting to destroy on removed layer: %s", layer->getName().string());
        return;
    }

    if (state.what & layer_state_t::eDestroySurface)
    {
        removeLayerLocked(mStateLock, layer);
    }
}

status_t SurfaceFlinger::createLayer(
    const String8 &name,
    const sp<Client> &client,
    uint32_t w, uint32_t h, PixelFormat format, uint32_t flags,
    int32_t windowType, int32_t ownerUid, sp<IBinder> *handle,
    sp<IGraphicBufferProducer> *gbp, sp<Layer> *parent)
{
    if (int32_t(w | h) < 0)
    {
        ALOGE("createLayer() failed, w or h is negative (w=%d, h=%d)",
              int(w), int(h));
        return BAD_VALUE;
    }

    status_t result = NO_ERROR;

    sp<Layer> layer;

    String8 uniqueName = getUniqueLayerName(name);

    switch (flags & ISurfaceComposerClient::eFXSurfaceMask)
    {
    case ISurfaceComposerClient::eFXSurfaceNormal:
        result = createBufferLayer(client,
                                   uniqueName, w, h, flags, format,
                                   handle, gbp, &layer);

        break;
    case ISurfaceComposerClient::eFXSurfaceColor:
        result = createColorLayer(client,
                                  uniqueName, w, h, flags,
                                  handle, &layer);
        break;
    default:
        result = BAD_VALUE;
        break;
    }

    if (result != NO_ERROR)
    {
        return result;
    }

    // window type is WINDOW_TYPE_DONT_SCREENSHOT from SurfaceControl.java
    // TODO b/64227542
    if (windowType == 441731)
    {
        windowType = 2024; // TYPE_NAVIGATION_BAR_PANEL
        layer->setPrimaryDisplayOnly();
    }

    layer->setInfo(windowType, ownerUid);

    result = addClientLayer(client, *handle, *gbp, layer, *parent);
    if (result != NO_ERROR)
    {
        return result;
    }
    mInterceptor->saveSurfaceCreation(layer);

    setTransactionFlags(eTransactionNeeded);
    return result;
}

String8 SurfaceFlinger::getUniqueLayerName(const String8 &name)
{
    bool matchFound = true;
    uint32_t dupeCounter = 0;

    // Tack on our counter whether there is a hit or not, so everyone gets a tag
    String8 uniqueName = name + "#" + String8(std::to_string(dupeCounter).c_str());

    // Grab the state lock since we're accessing mCurrentState
    Mutex::Autolock lock(mStateLock);

    // Loop over layers until we're sure there is no matching name
    while (matchFound)
    {
        matchFound = false;
        mCurrentState.traverseInZOrder([&](Layer *layer) {
            if (layer->getName() == uniqueName)
            {
                matchFound = true;
                uniqueName = name + "#" + String8(std::to_string(++dupeCounter).c_str());
            }
        });
    }

    ALOGD_IF(dupeCounter > 0, "duplicate layer name: changing %s to %s", name.c_str(), uniqueName.c_str());

    return uniqueName;
}

status_t SurfaceFlinger::createBufferLayer(const sp<Client> &client,
                                           const String8 &name, uint32_t w, uint32_t h, uint32_t flags, PixelFormat &format,
                                           sp<IBinder> *handle, sp<IGraphicBufferProducer> *gbp, sp<Layer> *outLayer)
{
    // initialize the surfaces
    switch (format)
    {
    case PIXEL_FORMAT_TRANSPARENT:
    case PIXEL_FORMAT_TRANSLUCENT:
        format = PIXEL_FORMAT_RGBA_8888;
        break;
    case PIXEL_FORMAT_OPAQUE:
        format = PIXEL_FORMAT_RGBX_8888;
        break;
    }

    sp<BufferLayer> layer = new BufferLayer(this, client, name, w, h, flags);
    status_t err = layer->setBuffers(w, h, format, flags);
    if (err == NO_ERROR)
    {
        *handle = layer->getHandle();
        *gbp = layer->getProducer();
        *outLayer = layer;
    }

    ALOGE_IF(err, "createBufferLayer() failed (%s)", strerror(-err));
    return err;
}

status_t SurfaceFlinger::createColorLayer(const sp<Client> &client,
                                          const String8 &name, uint32_t w, uint32_t h, uint32_t flags,
                                          sp<IBinder> *handle, sp<Layer> *outLayer)
{
    *outLayer = new ColorLayer(this, client, name, w, h, flags);
    *handle = (*outLayer)->getHandle();
    return NO_ERROR;
}

status_t SurfaceFlinger::onLayerRemoved(const sp<Client> &client, const sp<IBinder> &handle)
{
    // called by a client when it wants to remove a Layer
    status_t err = NO_ERROR;
    sp<Layer> l(client->getLayerUser(handle));
    if (l != nullptr)
    {
        mInterceptor->saveSurfaceDeletion(l);
        err = removeLayer(l);
        ALOGE_IF(err < 0 && err != NAME_NOT_FOUND,
                 "error removing layer=%p (%s)", l.get(), strerror(-err));
    }
    return err;
}

status_t SurfaceFlinger::onLayerDestroyed(const wp<Layer> &layer)
{
    // called by ~LayerCleaner() when all references to the IBinder (handle)
    // are gone
    sp<Layer> l = layer.promote();
    if (l == nullptr)
    {
        // The layer has already been removed, carry on
        return NO_ERROR;
    }
    // If we have a parent, then we can continue to live as long as it does.
    return removeLayer(l, true);
}

// ---------------------------------------------------------------------------

void SurfaceFlinger::onInitializeDisplays()
{
    // reset screen orientation and use primary layer stack
    Vector<ComposerState> state;
    Vector<DisplayState> displays;
    DisplayState d;
    d.what = DisplayState::eDisplayProjectionChanged |
             DisplayState::eLayerStackChanged;
    d.token = mBuiltinDisplays[DisplayDevice::DISPLAY_PRIMARY];
    d.layerStack = 0;
    d.orientation = DisplayState::eOrientationDefault;
    d.frame.makeInvalid();
    d.viewport.makeInvalid();
    d.width = 0;
    d.height = 0;
    displays.add(d);
    setTransactionState(state, displays, 0);
    setPowerModeInternal(getDisplayDevice(d.token), HWC_POWER_MODE_NORMAL,
                         /*stateLockHeld*/ false);

    const auto &activeConfig = getBE().mHwc->getActiveConfig(HWC_DISPLAY_PRIMARY);
    const nsecs_t period = activeConfig->getVsyncPeriod();
    mAnimFrameTracker.setDisplayRefreshPeriod(period);

    // Use phase of 0 since phase is not known.
    // Use latency of 0, which will snap to the ideal latency.
    setCompositorTimingSnapped(0, period, 0);
}

void SurfaceFlinger::initializeDisplays()
{
    class MessageScreenInitialized : public MessageBase
    {
        SurfaceFlinger *flinger;

    public:
        explicit MessageScreenInitialized(SurfaceFlinger *flinger) : flinger(flinger) {}
        virtual bool handler()
        {
            flinger->onInitializeDisplays();
            return true;
        }
    };
    sp<MessageBase> msg = new MessageScreenInitialized(this);
    postMessageAsync(msg); // we may be called from main thread, use async message
}

void SurfaceFlinger::setPowerModeInternal(const sp<DisplayDevice> &hw,
                                          int mode, bool stateLockHeld)
{
    ALOGD("Set power mode=%d, type=%d flinger=%p", mode, hw->getDisplayType(),
          this);
    int32_t type = hw->getDisplayType();
    int currentMode = hw->getPowerMode();

    if (mode == currentMode)
    {
        return;
    }

    hw->setPowerMode(mode);
    if (type >= DisplayDevice::NUM_BUILTIN_DISPLAY_TYPES)
    {
        ALOGW("Trying to set power mode for virtual display");
        return;
    }

    if (mInterceptor->isEnabled())
    {
        ConditionalLock lock(mStateLock, !stateLockHeld);
        ssize_t idx = mCurrentState.displays.indexOfKey(hw->getDisplayToken());
        if (idx < 0)
        {
            ALOGW("Surface Interceptor SavePowerMode: invalid display token");
            return;
        }
        mInterceptor->savePowerModeUpdate(mCurrentState.displays.valueAt(idx).displayId, mode);
    }

    if (currentMode == HWC_POWER_MODE_OFF)
    {
        // Turn on the display
        getHwComposer().setPowerMode(type, mode);
        if (type == DisplayDevice::DISPLAY_PRIMARY &&
            mode != HWC_POWER_MODE_DOZE_SUSPEND)
        {
            // FIXME: eventthread only knows about the main display right now
            mEventThread->onScreenAcquired();
            resyncToHardwareVsync(true);
        }

        mVisibleRegionsDirty = true;
        mHasPoweredOff = true;
        repaintEverything();

        struct sched_param param = {0};
        param.sched_priority = 1;
        if (sched_setscheduler(0, SCHED_FIFO, &param) != 0)
        {
            ALOGW("Couldn't set SCHED_FIFO on display on");
        }
    }
    else if (mode == HWC_POWER_MODE_OFF)
    {
        // Turn off the display
        struct sched_param param = {0};
        if (sched_setscheduler(0, SCHED_OTHER, &param) != 0)
        {
            ALOGW("Couldn't set SCHED_OTHER on display off");
        }

        if (type == DisplayDevice::DISPLAY_PRIMARY &&
            currentMode != HWC_POWER_MODE_DOZE_SUSPEND)
        {
            disableHardwareVsync(true); // also cancels any in-progress resync

            // FIXME: eventthread only knows about the main display right now
            mEventThread->onScreenReleased();
        }

        getHwComposer().setPowerMode(type, mode);
        mVisibleRegionsDirty = true;
        // from this point on, SF will stop drawing on this display
    }
    else if (mode == HWC_POWER_MODE_DOZE ||
             mode == HWC_POWER_MODE_NORMAL)
    {
        // Update display while dozing
        getHwComposer().setPowerMode(type, mode);
        if (type == DisplayDevice::DISPLAY_PRIMARY &&
            currentMode == HWC_POWER_MODE_DOZE_SUSPEND)
        {
            // FIXME: eventthread only knows about the main display right now
            mEventThread->onScreenAcquired();
            resyncToHardwareVsync(true);
        }
    }
    else if (mode == HWC_POWER_MODE_DOZE_SUSPEND)
    {
        // Leave display going to doze
        if (type == DisplayDevice::DISPLAY_PRIMARY)
        {
            disableHardwareVsync(true); // also cancels any in-progress resync
            // FIXME: eventthread only knows about the main display right now
            mEventThread->onScreenReleased();
        }
        getHwComposer().setPowerMode(type, mode);
    }
    else
    {
        ALOGE("Attempting to set unknown power mode: %d\n", mode);
        getHwComposer().setPowerMode(type, mode);
    }
    ALOGD("Finished set power mode=%d, type=%d", mode, hw->getDisplayType());
}

void SurfaceFlinger::setPowerMode(const sp<IBinder> &display, int mode)
{
    class MessageSetPowerMode : public MessageBase
    {
        SurfaceFlinger &mFlinger;
        sp<IBinder> mDisplay;
        int mMode;

    public:
        MessageSetPowerMode(SurfaceFlinger &flinger,
                            const sp<IBinder> &disp, int mode) : mFlinger(flinger),
                                                                 mDisplay(disp) { mMode = mode; }
        virtual bool handler()
        {
            sp<DisplayDevice> hw(mFlinger.getDisplayDevice(mDisplay));
            if (hw == nullptr)
            {
                ALOGE("Attempt to set power mode = %d for null display %p",
                      mMode, mDisplay.get());
            }
            else if (hw->getDisplayType() >= DisplayDevice::DISPLAY_VIRTUAL)
            {
                ALOGW("Attempt to set power mode = %d for virtual display",
                      mMode);
            }
            else
            {
                mFlinger.setPowerModeInternal(
                    hw, mMode, /*stateLockHeld*/ false);
            }
            return true;
        }
    };
    sp<MessageBase> msg = new MessageSetPowerMode(*this, display, mode);
    postMessageSync(msg);
}

// ---------------------------------------------------------------------------

status_t SurfaceFlinger::doDump(int fd, const Vector<String16> &args, bool asProto)
    NO_THREAD_SAFETY_ANALYSIS
{
    String8 result;

    IPCThreadState *ipc = IPCThreadState::self();
    const int pid = ipc->getCallingPid();
    const int uid = ipc->getCallingUid();

    if ((uid != AID_SHELL) &&
        !PermissionCache::checkPermission(sDump, pid, uid))
    {
        result.appendFormat("Permission Denial: "
                            "can't dump SurfaceFlinger from pid=%d, uid=%d\n",
                            pid, uid);
    }
    else
    {
        // Try to get the main lock, but give up after one second
        // (this would indicate SF is stuck, but we want to be able to
        // print something in dumpsys).
        status_t err = mStateLock.timedLock(s2ns(1));
        bool locked = (err == NO_ERROR);
        if (!locked)
        {
            result.appendFormat(
                "SurfaceFlinger appears to be unresponsive (%s [%d]), "
                "dumping anyways (no locks held)\n",
                strerror(-err), err);
        }

        bool dumpAll = true;
        size_t index = 0;
        size_t numArgs = args.size();

        if (numArgs)
        {
            if ((index < numArgs) &&
                (args[index] == String16("--list")))
            {
                index++;
                listLayersLocked(args, index, result);
                dumpAll = false;
            }

            if ((index < numArgs) &&
                (args[index] == String16("--latency")))
            {
                index++;
                dumpStatsLocked(args, index, result);
                dumpAll = false;
            }

            if ((index < numArgs) &&
                (args[index] == String16("--latency-clear")))
            {
                index++;
                clearStatsLocked(args, index, result);
                dumpAll = false;
            }

            if ((index < numArgs) &&
                (args[index] == String16("--dispsync")))
            {
                index++;
                mPrimaryDispSync.dump(result);
                dumpAll = false;
            }

            if ((index < numArgs) &&
                (args[index] == String16("--static-screen")))
            {
                index++;
                dumpStaticScreenStats(result);
                dumpAll = false;
            }

            if ((index < numArgs) &&
                (args[index] == String16("--frame-events")))
            {
                index++;
                dumpFrameEventsLocked(result);
                dumpAll = false;
            }

            if ((index < numArgs) && (args[index] == String16("--wide-color")))
            {
                index++;
                dumpWideColorInfo(result);
                dumpAll = false;
            }

            if ((index < numArgs) &&
                (args[index] == String16("--enable-layer-stats")))
            {
                index++;
                mLayerStats.enable();
                dumpAll = false;
            }

            if ((index < numArgs) &&
                (args[index] == String16("--disable-layer-stats")))
            {
                index++;
                mLayerStats.disable();
                dumpAll = false;
            }

            if ((index < numArgs) &&
                (args[index] == String16("--clear-layer-stats")))
            {
                index++;
                mLayerStats.clear();
                dumpAll = false;
            }

            if ((index < numArgs) &&
                (args[index] == String16("--dump-layer-stats")))
            {
                index++;
                mLayerStats.dump(result);
                dumpAll = false;
            }

            if ((index < numArgs) && (args[index] == String16("--timestats")))
            {
                index++;
                mTimeStats.parseArgs(asProto, args, index, result);
                dumpAll = false;
            }
        }

        if (dumpAll)
        {
            if (asProto)
            {
                LayersProto layersProto = dumpProtoInfo(LayerVector::StateSet::Current);
                result.append(layersProto.SerializeAsString().c_str(), layersProto.ByteSize());
            }
            else
            {
                dumpAllLocked(args, index, result);
            }
        }

        if (locked)
        {
            mStateLock.unlock();
        }
    }
    write(fd, result.string(), result.size());
    return NO_ERROR;
}

void SurfaceFlinger::listLayersLocked(const Vector<String16> & /* args */,
                                      size_t & /* index */, String8 &result) const
{
    mCurrentState.traverseInZOrder([&](Layer *layer) {
        result.appendFormat("%s\n", layer->getName().string());
    });
}

void SurfaceFlinger::dumpStatsLocked(const Vector<String16> &args, size_t &index,
                                     String8 &result) const
{
    String8 name;
    if (index < args.size())
    {
        name = String8(args[index]);
        index++;
    }

    const auto &activeConfig = getBE().mHwc->getActiveConfig(HWC_DISPLAY_PRIMARY);
    const nsecs_t period = activeConfig->getVsyncPeriod();
    result.appendFormat("%" PRId64 "\n", period);

    if (name.isEmpty())
    {
        mAnimFrameTracker.dumpStats(result);
    }
    else
    {
        mCurrentState.traverseInZOrder([&](Layer *layer) {
            if (name == layer->getName())
            {
                layer->dumpFrameStats(result);
            }
        });
    }
}

void SurfaceFlinger::clearStatsLocked(const Vector<String16> &args, size_t &index,
                                      String8 & /* result */)
{
    String8 name;
    if (index < args.size())
    {
        name = String8(args[index]);
        index++;
    }

    mCurrentState.traverseInZOrder([&](Layer *layer) {
        if (name.isEmpty() || (name == layer->getName()))
        {
            layer->clearFrameStats();
        }
    });

    mAnimFrameTracker.clearStats();
}

// This should only be called from the main thread.  Otherwise it would need
// the lock and should use mCurrentState rather than mDrawingState.
void SurfaceFlinger::logFrameStats()
{
    mDrawingState.traverseInZOrder([&](Layer *layer) {
        layer->logFrameStats();
    });

    mAnimFrameTracker.logAndResetStats(String8("<win-anim>"));
}

void SurfaceFlinger::appendSfConfigString(String8 &result) const
{
    result.append(" [sf");

    if (isLayerTripleBufferingDisabled())
        result.append(" DISABLE_TRIPLE_BUFFERING");

    result.appendFormat(" PRESENT_TIME_OFFSET=%" PRId64, dispSyncPresentTimeOffset);
    result.appendFormat(" FORCE_HWC_FOR_RBG_TO_YUV=%d", useHwcForRgbToYuv);
    result.appendFormat(" MAX_VIRT_DISPLAY_DIM=%" PRIu64, maxVirtualDisplaySize);
    result.appendFormat(" RUNNING_WITHOUT_SYNC_FRAMEWORK=%d", !hasSyncFramework);
    result.appendFormat(" NUM_FRAMEBUFFER_SURFACE_BUFFERS=%" PRId64,
                        maxFrameBufferAcquiredBuffers);
    result.append("]");
}

void SurfaceFlinger::dumpStaticScreenStats(String8 &result) const
{
    result.appendFormat("Static screen stats:\n");
    for (size_t b = 0; b < SurfaceFlingerBE::NUM_BUCKETS - 1; ++b)
    {
        float bucketTimeSec = getBE().mFrameBuckets[b] / 1e9;
        float percent = 100.0f *
                        static_cast<float>(getBE().mFrameBuckets[b]) / getBE().mTotalTime;
        result.appendFormat("  < %zd frames: %.3f s (%.1f%%)\n",
                            b + 1, bucketTimeSec, percent);
    }
    float bucketTimeSec = getBE().mFrameBuckets[SurfaceFlingerBE::NUM_BUCKETS - 1] / 1e9;
    float percent = 100.0f *
                    static_cast<float>(getBE().mFrameBuckets[SurfaceFlingerBE::NUM_BUCKETS - 1]) / getBE().mTotalTime;
    result.appendFormat("  %zd+ frames: %.3f s (%.1f%%)\n",
                        SurfaceFlingerBE::NUM_BUCKETS - 1, bucketTimeSec, percent);
}

void SurfaceFlinger::recordBufferingStats(const char *layerName,
                                          std::vector<OccupancyTracker::Segment> &&history)
{
    Mutex::Autolock lock(getBE().mBufferingStatsMutex);
    auto &stats = getBE().mBufferingStats[layerName];
    for (const auto &segment : history)
    {
        if (!segment.usedThirdBuffer)
        {
            stats.twoBufferTime += segment.totalTime;
        }
        if (segment.occupancyAverage < 1.0f)
        {
            stats.doubleBufferedTime += segment.totalTime;
        }
        else if (segment.occupancyAverage < 2.0f)
        {
            stats.tripleBufferedTime += segment.totalTime;
        }
        ++stats.numSegments;
        stats.totalTime += segment.totalTime;
    }
}

void SurfaceFlinger::dumpFrameEventsLocked(String8 &result)
{
    result.appendFormat("Layer frame timestamps:\n");

    const LayerVector &currentLayers = mCurrentState.layersSortedByZ;
    const size_t count = currentLayers.size();
    for (size_t i = 0; i < count; i++)
    {
        currentLayers[i]->dumpFrameEvents(result);
    }
}

void SurfaceFlinger::dumpBufferingStats(String8 &result) const
{
    result.append("Buffering stats:\n");
    result.append("  [Layer name] <Active time> <Two buffer> "
                  "<Double buffered> <Triple buffered>\n");
    Mutex::Autolock lock(getBE().mBufferingStatsMutex);
    typedef std::tuple<std::string, float, float, float> BufferTuple;
    std::map<float, BufferTuple, std::greater<float>> sorted;
    for (const auto &statsPair : getBE().mBufferingStats)
    {
        const char *name = statsPair.first.c_str();
        const SurfaceFlingerBE::BufferingStats &stats = statsPair.second;
        if (stats.numSegments == 0)
        {
            continue;
        }
        float activeTime = ns2ms(stats.totalTime) / 1000.0f;
        float twoBufferRatio = static_cast<float>(stats.twoBufferTime) /
                               stats.totalTime;
        float doubleBufferRatio = static_cast<float>(
                                      stats.doubleBufferedTime) /
                                  stats.totalTime;
        float tripleBufferRatio = static_cast<float>(
                                      stats.tripleBufferedTime) /
                                  stats.totalTime;
        sorted.insert({activeTime, {name, twoBufferRatio, doubleBufferRatio, tripleBufferRatio}});
    }
    for (const auto &sortedPair : sorted)
    {
        float activeTime = sortedPair.first;
        const BufferTuple &values = sortedPair.second;
        result.appendFormat("  [%s] %.2f %.3f %.3f %.3f\n",
                            std::get<0>(values).c_str(), activeTime,
                            std::get<1>(values), std::get<2>(values),
                            std::get<3>(values));
    }
    result.append("\n");
}

void SurfaceFlinger::dumpWideColorInfo(String8 &result) const
{
    result.appendFormat("hasWideColorDisplay: %d\n", hasWideColorDisplay);
    result.appendFormat("DisplayColorSetting: %s\n",
                        decodeDisplayColorSetting(mDisplayColorSetting).c_str());

    // TODO: print out if wide-color mode is active or not

    for (size_t d = 0; d < mDisplays.size(); d++)
    {
        const sp<const DisplayDevice> &displayDevice(mDisplays[d]);
        int32_t hwcId = displayDevice->getHwcDisplayId();
        if (hwcId == DisplayDevice::DISPLAY_ID_INVALID)
        {
            continue;
        }

        result.appendFormat("Display %d color modes:\n", hwcId);
        std::vector<ColorMode> modes = getHwComposer().getColorModes(hwcId);
        for (auto &&mode : modes)
        {
            result.appendFormat("    %s (%d)\n", decodeColorMode(mode).c_str(), mode);
        }

        ColorMode currentMode = displayDevice->getActiveColorMode();
        result.appendFormat("    Current color mode: %s (%d)\n",
                            decodeColorMode(currentMode).c_str(), currentMode);
    }
    result.append("\n");
}

LayersProto SurfaceFlinger::dumpProtoInfo(LayerVector::StateSet stateSet) const
{
    LayersProto layersProto;
    const bool useDrawing = stateSet == LayerVector::StateSet::Drawing;
    const State &state = useDrawing ? mDrawingState : mCurrentState;
    state.traverseInZOrder([&](Layer *layer) {
        LayerProto *layerProto = layersProto.add_layers();
        layer->writeToProto(layerProto, stateSet);
    });

    return layersProto;
}

LayersProto SurfaceFlinger::dumpVisibleLayersProtoInfo(int32_t hwcId) const
{
    LayersProto layersProto;
    const sp<DisplayDevice> &displayDevice(mDisplays[hwcId]);

    SizeProto *resolution = layersProto.mutable_resolution();
    resolution->set_w(displayDevice->getWidth());
    resolution->set_h(displayDevice->getHeight());

    layersProto.set_color_mode(decodeColorMode(displayDevice->getActiveColorMode()));
    layersProto.set_color_transform(decodeColorTransform(displayDevice->getColorTransform()));
    layersProto.set_global_transform(
        static_cast<int32_t>(displayDevice->getOrientationTransform()));

    mDrawingState.traverseInZOrder([&](Layer *layer) {
        if (!layer->visibleRegion.isEmpty() && layer->getBE().mHwcLayers.count(hwcId))
        {
            LayerProto *layerProto = layersProto.add_layers();
            layer->writeToProto(layerProto, hwcId);
        }
    });

    return layersProto;
}

void SurfaceFlinger::dumpAllLocked(const Vector<String16> &args, size_t &index,
                                   String8 &result) const
{
    bool colorize = false;
    if (index < args.size() && (args[index] == String16("--color")))
    {
        colorize = true;
        index++;
    }

    Colorizer colorizer(colorize);

    // figure out if we're stuck somewhere
    const nsecs_t now = systemTime();
    const nsecs_t inSwapBuffers(mDebugInSwapBuffers);
    const nsecs_t inTransaction(mDebugInTransaction);
    nsecs_t inSwapBuffersDuration = (inSwapBuffers) ? now - inSwapBuffers : 0;
    nsecs_t inTransactionDuration = (inTransaction) ? now - inTransaction : 0;

    /*
     * Dump library configuration.
     */

    colorizer.bold(result);
    result.append("Build configuration:");
    colorizer.reset(result);
    appendSfConfigString(result);
    appendUiConfigString(result);
    appendGuiConfigString(result);
    result.append("\n");

    result.append("\nWide-Color information:\n");
    dumpWideColorInfo(result);

    colorizer.bold(result);
    result.append("Sync configuration: ");
    colorizer.reset(result);
    result.append(SyncFeatures::getInstance().toString());
    result.append("\n");

    const auto &activeConfig = getBE().mHwc->getActiveConfig(HWC_DISPLAY_PRIMARY);

    colorizer.bold(result);
    result.append("DispSync configuration: ");
    colorizer.reset(result);
    const auto [sfEarlyOffset, appEarlyOffset] = mVsyncModulator.getEarlyOffsets();
    const auto [sfEarlyGlOffset, appEarlyGlOffset] = mVsyncModulator.getEarlyGlOffsets();
    result.appendFormat(
        "app phase %" PRId64 " ns, "
        "sf phase %" PRId64 " ns, "
        "early app phase %" PRId64 " ns, "
        "early sf phase %" PRId64 " ns, "
        "early app gl phase %" PRId64 " ns, "
        "early sf gl phase %" PRId64 " ns, "
        "present offset %" PRId64 " ns (refresh %" PRId64 " ns)",
        vsyncPhaseOffsetNs,
        sfVsyncPhaseOffsetNs,
        appEarlyOffset,
        sfEarlyOffset,
        appEarlyGlOffset,
        sfEarlyOffset,
        dispSyncPresentTimeOffset, activeConfig->getVsyncPeriod());
    result.append("\n");

    // Dump static screen stats
    result.append("\n");
    dumpStaticScreenStats(result);
    result.append("\n");

    dumpBufferingStats(result);

    /*
     * Dump the visible layer list
     */
    colorizer.bold(result);
    result.appendFormat("Visible layers (count = %zu)\n", mNumLayers);
    result.appendFormat("GraphicBufferProducers: %zu, max %zu\n",
                        mGraphicBufferProducerList.size(), mMaxGraphicBufferProducerListSize);
    colorizer.reset(result);

    LayersProto layersProto = dumpProtoInfo(LayerVector::StateSet::Current);
    auto layerTree = LayerProtoParser::generateLayerTree(layersProto);
    result.append(LayerProtoParser::layersToString(std::move(layerTree)).c_str());
    result.append("\n");

    /*
     * Dump Display state
     */

    colorizer.bold(result);
    result.appendFormat("Displays (%zu entries)\n", mDisplays.size());
    colorizer.reset(result);
    for (size_t dpy = 0; dpy < mDisplays.size(); dpy++)
    {
        const sp<const DisplayDevice> &hw(mDisplays[dpy]);
        hw->dump(result);
    }
    result.append("\n");

    /*
     * Dump SurfaceFlinger global state
     */

    colorizer.bold(result);
    result.append("SurfaceFlinger global state:\n");
    colorizer.reset(result);

    HWComposer &hwc(getHwComposer());
    sp<const DisplayDevice> hw(getDefaultDisplayDeviceLocked());

    getBE().mRenderEngine->dump(result);

    if (hw)
    {
        hw->undefinedRegion.dump(result, "undefinedRegion");
        result.appendFormat("  orientation=%d, isDisplayOn=%d\n",
                            hw->getOrientation(), hw->isDisplayOn());
    }
    result.appendFormat(
        "  last eglSwapBuffers() time: %f us\n"
        "  last transaction time     : %f us\n"
        "  transaction-flags         : %08x\n"
        "  refresh-rate              : %f fps\n"
        "  x-dpi                     : %f\n"
        "  y-dpi                     : %f\n"
        "  gpu_to_cpu_unsupported    : %d\n",
        mLastSwapBufferTime / 1000.0,
        mLastTransactionTime / 1000.0,
        mTransactionFlags,
        1e9 / activeConfig->getVsyncPeriod(),
        activeConfig->getDpiX(),
        activeConfig->getDpiY(),
        !mGpuToCpuSupported);

    result.appendFormat("  eglSwapBuffers time: %f us\n",
                        inSwapBuffersDuration / 1000.0);

    result.appendFormat("  transaction time: %f us\n",
                        inTransactionDuration / 1000.0);

    /*
     * VSYNC state
     */
    mEventThread->dump(result);
    result.append("\n");

    /*
     * Tracing state
     */
    mTracing.dump(result);
    result.append("\n");

    /*
     * HWC layer minidump
     */
    for (size_t d = 0; d < mDisplays.size(); d++)
    {
        const sp<const DisplayDevice> &displayDevice(mDisplays[d]);
        int32_t hwcId = displayDevice->getHwcDisplayId();
        if (hwcId == DisplayDevice::DISPLAY_ID_INVALID)
        {
            continue;
        }

        result.appendFormat("Display %d HWC layers:\n", hwcId);
        Layer::miniDumpHeader(result);
        mCurrentState.traverseInZOrder([&](Layer *layer) {
            layer->miniDump(result, hwcId);
        });
        result.append("\n");
    }

    /*
     * Dump HWComposer state
     */
    colorizer.bold(result);
    result.append("h/w composer state:\n");
    colorizer.reset(result);
    bool hwcDisabled = mDebugDisableHWC || mDebugRegion;
    result.appendFormat("  h/w composer %s\n",
                        hwcDisabled ? "disabled" : "enabled");
    hwc.dump(result);

    /*
     * Dump gralloc state
     */
    const GraphicBufferAllocator &alloc(GraphicBufferAllocator::get());
    alloc.dump(result);

    /*
     * Dump VrFlinger state if in use.
     */
    if (mVrFlingerRequestsDisplay && mVrFlinger)
    {
        result.append("VrFlinger state:\n");
        result.append(mVrFlinger->Dump().c_str());
        result.append("\n");
    }
}

const Vector<sp<Layer>> &
SurfaceFlinger::getLayerSortedByZForHwcDisplay(int id)
{
    // Note: mStateLock is held here
    wp<IBinder> dpy;
    for (size_t i = 0; i < mDisplays.size(); i++)
    {
        if (mDisplays.valueAt(i)->getHwcDisplayId() == id)
        {
            dpy = mDisplays.keyAt(i);
            break;
        }
    }
    if (dpy == nullptr)
    {
        ALOGE("getLayerSortedByZForHwcDisplay: invalid hwc display id %d", id);
        // Just use the primary display so we have something to return
        dpy = getBuiltInDisplay(DisplayDevice::DISPLAY_PRIMARY);
    }
    return getDisplayDeviceLocked(dpy)->getVisibleLayersSortedByZ();
}

bool SurfaceFlinger::startDdmConnection()
{
    void *libddmconnection_dso =
        dlopen("libsurfaceflinger_ddmconnection.so", RTLD_NOW);
    if (!libddmconnection_dso)
    {
        return false;
    }
    void (*DdmConnection_start)(const char *name);
    DdmConnection_start =
        (decltype(DdmConnection_start))dlsym(libddmconnection_dso, "DdmConnection_start");
    if (!DdmConnection_start)
    {
        dlclose(libddmconnection_dso);
        return false;
    }
    (*DdmConnection_start)(getServiceName());
    return true;
}

void SurfaceFlinger::updateColorMatrixLocked()
{
    mat4 colorMatrix;
    if (mGlobalSaturationFactor != 1.0f)
    {
        // Rec.709 luma coefficients
        float3 luminance{0.213f, 0.715f, 0.072f};
        luminance *= 1.0f - mGlobalSaturationFactor;
        mat4 saturationMatrix = mat4(
            vec4{luminance.r + mGlobalSaturationFactor, luminance.r, luminance.r, 0.0f},
            vec4{luminance.g, luminance.g + mGlobalSaturationFactor, luminance.g, 0.0f},
            vec4{luminance.b, luminance.b, luminance.b + mGlobalSaturationFactor, 0.0f},
            vec4{0.0f, 0.0f, 0.0f, 1.0f});
        colorMatrix = mClientColorMatrix * saturationMatrix * mDaltonizer();
    }
    else
    {
        colorMatrix = mClientColorMatrix * mDaltonizer();
    }

    if (mCurrentState.colorMatrix != colorMatrix)
    {
        mCurrentState.colorMatrix = colorMatrix;
        mCurrentState.colorMatrixChanged = true;
        setTransactionFlags(eTransactionNeeded);
    }
}

status_t SurfaceFlinger::CheckTransactCodeCredentials(uint32_t code)
{
    switch (code)
    {
    case CREATE_CONNECTION:
    case CREATE_DISPLAY:
    case BOOT_FINISHED:
    case CLEAR_ANIMATION_FRAME_STATS:
    case GET_ANIMATION_FRAME_STATS:
    case SET_POWER_MODE:
    case GET_HDR_CAPABILITIES:
    case ENABLE_VSYNC_INJECTIONS:
    case INJECT_VSYNC:
    {
        // codes that require permission check
        if (!callingThreadHasUnscopedSurfaceFlingerAccess())
        {
            IPCThreadState *ipc = IPCThreadState::self();
            ALOGE("Permission Denial: can't access SurfaceFlinger pid=%d, uid=%d",
                  ipc->getCallingPid(), ipc->getCallingUid());
            return PERMISSION_DENIED;
        }
        break;
    }
    /*
         * Calling setTransactionState is safe, because you need to have been
         * granted a reference to Client* and Handle* to do anything with it.
         *
         * Creating a scoped connection is safe, as per discussion in ISurfaceComposer.h
         */
    case SET_TRANSACTION_STATE:
    case CREATE_SCOPED_CONNECTION:
    {
        return OK;
    }
    case CAPTURE_SCREEN:
    {
        // codes that require permission check
        IPCThreadState *ipc = IPCThreadState::self();
        const int pid = ipc->getCallingPid();
        const int uid = ipc->getCallingUid();
        if ((uid != AID_GRAPHICS) &&
            !PermissionCache::checkPermission(sReadFramebuffer, pid, uid))
        {
            ALOGE("Permission Denial: can't read framebuffer pid=%d, uid=%d", pid, uid);
            return PERMISSION_DENIED;
        }
        break;
    }
    case CAPTURE_LAYERS:
    {
        IPCThreadState *ipc = IPCThreadState::self();
        const int pid = ipc->getCallingPid();
        const int uid = ipc->getCallingUid();
        if ((uid != AID_GRAPHICS) &&
            !PermissionCache::checkPermission(sReadFramebuffer, pid, uid))
        {
            ALOGE("Permission Denial: can't read framebuffer pid=%d, uid=%d", pid, uid);
            return PERMISSION_DENIED;
        }
        break;
    }
    }
    return OK;
}

status_t SurfaceFlinger::onTransact(
    uint32_t code, const Parcel &data, Parcel *reply, uint32_t flags)
{
    int n;
    status_t err = BnSurfaceComposer::onTransact(code, data, reply, flags);
    switch (code)
    {
    case 1000: // SHOW_CPU, NOT SUPPORTED ANYMORE
    case 1001: // SHOW_FPS, NOT SUPPORTED ANYMORE
    {
        CHECK_INTERFACE(ISurfaceComposer, data, reply);
        return NO_ERROR;
    }
    case 1002: // SHOW_UPDATES
    {
        CHECK_INTERFACE(ISurfaceComposer, data, reply);
        n = data.readInt32();
        mDebugRegion = n ? n : (mDebugRegion ? 0 : 1);
        invalidateHwcGeometry();
        repaintEverything();
        return NO_ERROR;
    }
    case 1004:
    { // repaint everything
        CHECK_INTERFACE(ISurfaceComposer, data, reply);
        repaintEverything();
        return NO_ERROR;
    }
    case 1005:
    { // force transaction
        CHECK_INTERFACE(ISurfaceComposer, data, reply);
        Mutex::Autolock _l(mStateLock);
        setTransactionFlags(
            eTransactionNeeded |
            eDisplayTransactionNeeded |
            eTraversalNeeded);
        return NO_ERROR;
    }
    case 1006:
    { // send empty update
        CHECK_INTERFACE(ISurfaceComposer, data, reply);
        signalRefresh();
        return NO_ERROR;
    }
    case 1008: // toggle use of hw composer
    {
        CHECK_INTERFACE(ISurfaceComposer, data, reply);
        n = data.readInt32();
        mDebugDisableHWC = n ? 1 : 0;
        invalidateHwcGeometry();
        repaintEverything();
        return NO_ERROR;
    }
    case 1009: // toggle use of transform hint
    {
        CHECK_INTERFACE(ISurfaceComposer, data, reply);
        n = data.readInt32();
        mDebugDisableTransformHint = n ? 1 : 0;
        invalidateHwcGeometry();
        repaintEverything();
        return NO_ERROR;
    }
    case 1010: // interrogate.
    {
        CHECK_INTERFACE(ISurfaceComposer, data, reply);
        reply->writeInt32(0);
        reply->writeInt32(0);
        reply->writeInt32(mDebugRegion);
        reply->writeInt32(0);
        reply->writeInt32(mDebugDisableHWC);
        return NO_ERROR;
    }
    case 1013:
    {
        CHECK_INTERFACE(ISurfaceComposer, data, reply);
        sp<const DisplayDevice> hw(getDefaultDisplayDevice());
        reply->writeInt32(hw->getPageFlipCount());
        return NO_ERROR;
    }
    case 1014:
    {
        CHECK_INTERFACE(ISurfaceComposer, data, reply);
        Mutex::Autolock _l(mStateLock);
        // daltonize
        n = data.readInt32();
        switch (n % 10)
        {
        case 1:
            mDaltonizer.setType(ColorBlindnessType::Protanomaly);
            break;
        case 2:
            mDaltonizer.setType(ColorBlindnessType::Deuteranomaly);
            break;
        case 3:
            mDaltonizer.setType(ColorBlindnessType::Tritanomaly);
            break;
        default:
            mDaltonizer.setType(ColorBlindnessType::None);
            break;
        }
        if (n >= 10)
        {
            mDaltonizer.setMode(ColorBlindnessMode::Correction);
        }
        else
        {
            mDaltonizer.setMode(ColorBlindnessMode::Simulation);
        }

        updateColorMatrixLocked();
        return NO_ERROR;
    }
    case 1015:
    {
        CHECK_INTERFACE(ISurfaceComposer, data, reply);

        Mutex::Autolock _l(mStateLock);
        // apply a color matrix
        n = data.readInt32();
        if (n)
        {
            // color matrix is sent as a column-major mat4 matrix
            for (size_t i = 0; i < 4; i++)
            {
                for (size_t j = 0; j < 4; j++)
                {
                    mClientColorMatrix[i][j] = data.readFloat();
                }
            }
        }
        else
        {
            mClientColorMatrix = mat4();
        }

        // Check that supplied matrix's last row is {0,0,0,1} so we can avoid
        // the division by w in the fragment shader
        float4 lastRow(transpose(mClientColorMatrix)[3]);
        if (any(greaterThan(abs(lastRow - float4{0, 0, 0, 1}), float4{1e-4f})))
        {
            ALOGE("The color transform's last row must be (0, 0, 0, 1)");
        }

        updateColorMatrixLocked();
        return NO_ERROR;
    }
    // This is an experimental interface
    // Needs to be shifted to proper binder interface when we productize
    case 1016:
    {
        CHECK_INTERFACE(ISurfaceComposer, data, reply);

        n = data.readInt32();
        mPrimaryDispSync.setRefreshSkipCount(n);
        return NO_ERROR;
    }
    case 1017:
    {
        CHECK_INTERFACE(ISurfaceComposer, data, reply);

        n = data.readInt32();
        mForceFullDamage = static_cast<bool>(n);
        return NO_ERROR;
    }
    case 1018:
    { // Modify Choreographer's phase offset
        CHECK_INTERFACE(ISurfaceComposer, data, reply);

        n = data.readInt32();
        mEventThread->setPhaseOffset(static_cast<nsecs_t>(n));
        return NO_ERROR;
    }
    case 1019:
    { // Modify SurfaceFlinger's phase offset
        CHECK_INTERFACE(ISurfaceComposer, data, reply);

        n = data.readInt32();
        mSFEventThread->setPhaseOffset(static_cast<nsecs_t>(n));
        return NO_ERROR;
    }
    case 1020:
    { // Layer updates interceptor
        CHECK_INTERFACE(ISurfaceComposer, data, reply);

        n = data.readInt32();
        if (n)
        {
            ALOGV("Interceptor enabled");
            mInterceptor->enable(mDrawingState.layersSortedByZ, mDrawingState.displays);
        }
        else
        {
            ALOGV("Interceptor disabled");
            mInterceptor->disable();
        }
        return NO_ERROR;
    }
    case 1021:
    { // Disable HWC virtual displays
        CHECK_INTERFACE(ISurfaceComposer, data, reply);

        n = data.readInt32();
        mUseHwcVirtualDisplays = !n;
        return NO_ERROR;
    }
    case 1022:
    { // Set saturation boost
        CHECK_INTERFACE(ISurfaceComposer, data, reply);

        Mutex::Autolock _l(mStateLock);
        mGlobalSaturationFactor = std::max(0.0f, std::min(data.readFloat(), 2.0f));

        updateColorMatrixLocked();
        return NO_ERROR;
    }
    case 1023:
    { // Set native mode
        CHECK_INTERFACE(ISurfaceComposer, data, reply);

        mDisplayColorSetting = static_cast<DisplayColorSetting>(data.readInt32());
        invalidateHwcGeometry();
        repaintEverything();
        return NO_ERROR;
    }
    case 1024:
    { // Is wide color gamut rendering/color management supported?
        CHECK_INTERFACE(ISurfaceComposer, data, reply);

        reply->writeBool(hasWideColorDisplay);
        return NO_ERROR;
    }
    case 1025:
    { // Set layer tracing
        CHECK_INTERFACE(ISurfaceComposer, data, reply);

        n = data.readInt32();
        if (n)
        {
            ALOGD("LayerTracing enabled");
            mTracing.enable();
            doTracing("tracing.enable");
            reply->writeInt32(NO_ERROR);
        }
        else
        {
            ALOGD("LayerTracing disabled");
            status_t err = mTracing.disable();
            reply->writeInt32(err);
        }
        return NO_ERROR;
    }
    case 1026:
    { // Get layer tracing status
        CHECK_INTERFACE(ISurfaceComposer, data, reply);

        reply->writeBool(mTracing.isEnabled());
        return NO_ERROR;
    }
    // Is a DisplayColorSetting supported?
    case 1027:
    {
        CHECK_INTERFACE(ISurfaceComposer, data, reply);

        sp<const DisplayDevice> hw(getDefaultDisplayDevice());
        if (!hw)
        {
            return NAME_NOT_FOUND;
        }

        DisplayColorSetting setting = static_cast<DisplayColorSetting>(data.readInt32());
        switch (setting)
        {
        case DisplayColorSetting::MANAGED:
            reply->writeBool(hasWideColorDisplay);
            break;
        case DisplayColorSetting::UNMANAGED:
            reply->writeBool(true);
            break;
        case DisplayColorSetting::ENHANCED:
            reply->writeBool(hw->hasRenderIntent(RenderIntent::ENHANCE));
            break;
        default: // vendor display color setting
            reply->writeBool(hw->hasRenderIntent(static_cast<RenderIntent>(setting)));
            break;
        }
        return NO_ERROR;
    }
    }
    return err;
}

void SurfaceFlinger::repaintEverything()
{
    android_atomic_or(1, &mRepaintEverything);
    signalTransaction();
}

// A simple RAII class to disconnect from an ANativeWindow* when it goes out of scope
class WindowDisconnector
{
public:
    WindowDisconnector(ANativeWindow *window, int api) : mWindow(window), mApi(api) {}
    ~WindowDisconnector()
    {
        native_window_api_disconnect(mWindow, mApi);
    }

private:
    ANativeWindow *mWindow;
    const int mApi;
};

status_t SurfaceFlinger::captureScreen(const sp<IBinder> &display,
                                       sp<GraphicBuffer> *outBuffer, bool &outCapturedSecureLayers,
                                       Rect sourceCrop, uint32_t reqWidth, uint32_t reqHeight,
                                       int32_t minLayerZ, int32_t maxLayerZ,
                                       bool useIdentityTransform,
                                       ISurfaceComposer::Rotation rotation,
                                       bool captureSecureLayers)
{
    ATRACE_CALL();

    if (CC_UNLIKELY(display == 0))
        return BAD_VALUE;

    const sp<const DisplayDevice> device(getDisplayDeviceLocked(display));
    if (CC_UNLIKELY(device == 0))
        return BAD_VALUE;

    const Rect &dispScissor = device->getScissor();
    if (!dispScissor.isEmpty())
    {
        sourceCrop.set(dispScissor);
        // adb shell screencap will default reqWidth and reqHeight to zeros.
        if (reqWidth == 0 || reqHeight == 0)
        {
            reqWidth = uint32_t(device->getViewport().width());
            reqHeight = uint32_t(device->getViewport().height());
        }
    }

    DisplayRenderArea renderArea(device, sourceCrop, reqHeight, reqWidth, rotation,
                                 captureSecureLayers);

    auto traverseLayers = std::bind(std::mem_fn(&SurfaceFlinger::traverseLayersInDisplay), this,
                                    device, minLayerZ, maxLayerZ, std::placeholders::_1);
    return captureScreenCommon(renderArea, traverseLayers, outBuffer, useIdentityTransform,
                               outCapturedSecureLayers);
}

status_t SurfaceFlinger::captureLayers(const sp<IBinder> &layerHandleBinder,
                                       sp<GraphicBuffer> *outBuffer, const Rect &sourceCrop,
                                       float frameScale, bool childrenOnly)
{
    ATRACE_CALL();

    class LayerRenderArea : public RenderArea
    {
    public:
        LayerRenderArea(SurfaceFlinger *flinger, const sp<Layer> &layer, const Rect crop,
                        int32_t reqWidth, int32_t reqHeight, bool childrenOnly)
            : RenderArea(reqHeight, reqWidth, CaptureFill::CLEAR),
              mLayer(layer),
              mCrop(crop),
              mFlinger(flinger),
              mChildrenOnly(childrenOnly) {}
        const Transform &getTransform() const override { return mTransform; }
        Rect getBounds() const override
        {
            const Layer::State &layerState(mLayer->getDrawingState());
            return Rect(layerState.active.w, layerState.active.h);
        }
        int getHeight() const override { return mLayer->getDrawingState().active.h; }
        int getWidth() const override { return mLayer->getDrawingState().active.w; }
        bool isSecure() const override { return false; }
        bool needsFiltering() const override { return false; }
        Rect getSourceCrop() const override
        {
            if (mCrop.isEmpty())
            {
                return getBounds();
            }
            else
            {
                return mCrop;
            }
        }
        class ReparentForDrawing
        {
        public:
            const sp<Layer> &oldParent;
            const sp<Layer> &newParent;

            ReparentForDrawing(const sp<Layer> &oldParent, const sp<Layer> &newParent)
                : oldParent(oldParent), newParent(newParent)
            {
                oldParent->setChildrenDrawingParent(newParent);
            }
            ~ReparentForDrawing() { oldParent->setChildrenDrawingParent(oldParent); }
        };

        void render(std::function<void()> drawLayers) override
        {
            if (!mChildrenOnly)
            {
                mTransform = mLayer->getTransform().inverse();
                drawLayers();
            }
            else
            {
                Rect bounds = getBounds();
                screenshotParentLayer =
                    new ContainerLayer(mFlinger, nullptr, String8("Screenshot Parent"),
                                       bounds.getWidth(), bounds.getHeight(), 0);

                ReparentForDrawing reparent(mLayer, screenshotParentLayer);
                drawLayers();
            }
        }

    private:
        const sp<Layer> mLayer;
        const Rect mCrop;

        // In the "childrenOnly" case we reparent the children to a screenshot
        // layer which has no properties set and which does not draw.
        sp<ContainerLayer> screenshotParentLayer;
        Transform mTransform;

        SurfaceFlinger *mFlinger;
        const bool mChildrenOnly;
    };

    auto layerHandle = reinterpret_cast<Layer::Handle *>(layerHandleBinder.get());
    auto parent = layerHandle->owner.promote();

    if (parent == nullptr || parent->isPendingRemoval())
    {
        ALOGE("captureLayers called with a removed parent");
        return NAME_NOT_FOUND;
    }

    const int uid = IPCThreadState::self()->getCallingUid();
    const bool forSystem = uid == AID_GRAPHICS || uid == AID_SYSTEM;
    if (!forSystem && parent->getCurrentState().flags & layer_state_t::eLayerSecure)
    {
        ALOGW("Attempting to capture secure layer: PERMISSION_DENIED");
        return PERMISSION_DENIED;
    }

    Rect crop(sourceCrop);
    if (sourceCrop.width() <= 0)
    {
        crop.left = 0;
        crop.right = parent->getCurrentState().active.w;
    }

    if (sourceCrop.height() <= 0)
    {
        crop.top = 0;
        crop.bottom = parent->getCurrentState().active.h;
    }

    int32_t reqWidth = crop.width() * frameScale;
    int32_t reqHeight = crop.height() * frameScale;

    LayerRenderArea renderArea(this, parent, crop, reqWidth, reqHeight, childrenOnly);

    auto traverseLayers = [parent, childrenOnly](const LayerVector::Visitor &visitor) {
        parent->traverseChildrenInZOrder(LayerVector::StateSet::Drawing, [&](Layer *layer) {
            if (!layer->isVisible())
            {
                return;
            }
            else if (childrenOnly && layer == parent.get())
            {
                return;
            }
            visitor(layer);
        });
    };
    bool outCapturedSecureLayers = false;
    return captureScreenCommon(renderArea, traverseLayers, outBuffer, false,
                               outCapturedSecureLayers);
}

status_t SurfaceFlinger::captureScreenCommon(RenderArea &renderArea,
                                             TraverseLayersFunction traverseLayers,
                                             sp<GraphicBuffer> *outBuffer,
                                             bool useIdentityTransform,
                                             bool &outCapturedSecureLayers)
{
    ATRACE_CALL();

    renderArea.updateDimensions(mPrimaryDisplayOrientation);

    const uint32_t usage = GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN |
                           GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE;
    *outBuffer = new GraphicBuffer(renderArea.getReqWidth(), renderArea.getReqHeight(),
                                   HAL_PIXEL_FORMAT_RGBA_8888, 1, usage, "screenshot");

    // This mutex protects syncFd and captureResult for communication of the return values from the
    // main thread back to this Binder thread
    std::mutex captureMutex;
    std::condition_variable captureCondition;
    std::unique_lock<std::mutex> captureLock(captureMutex);
    int syncFd = -1;
    std::optional<status_t> captureResult;

    const int uid = IPCThreadState::self()->getCallingUid();
    const bool forSystem = uid == AID_GRAPHICS || uid == AID_SYSTEM;

    sp<LambdaMessage> message = new LambdaMessage([&]() {
        // If there is a refresh pending, bug out early and tell the binder thread to try again
        // after the refresh.
        if (mRefreshPending)
        {
            ATRACE_NAME("Skipping screenshot for now");
            std::unique_lock<std::mutex> captureLock(captureMutex);
            captureResult = std::make_optional<status_t>(EAGAIN);
            captureCondition.notify_one();
            return;
        }

        status_t result = NO_ERROR;
        int fd = -1;
        {
            Mutex::Autolock _l(mStateLock);
            renderArea.render([&]() {
                result = captureScreenImplLocked(renderArea, traverseLayers, (*outBuffer).get(),
                                                 useIdentityTransform, forSystem, &fd,
                                                 outCapturedSecureLayers);
            });
        }

        {
            std::unique_lock<std::mutex> captureLock(captureMutex);
            syncFd = fd;
            captureResult = std::make_optional<status_t>(result);
            captureCondition.notify_one();
        }
    });

    status_t result = postMessageAsync(message);
    if (result == NO_ERROR)
    {
        captureCondition.wait(captureLock, [&]() { return captureResult; });
        while (*captureResult == EAGAIN)
        {
            captureResult.reset();
            result = postMessageAsync(message);
            if (result != NO_ERROR)
            {
                return result;
            }
            captureCondition.wait(captureLock, [&]() { return captureResult; });
        }
        result = *captureResult;
    }

    if (result == NO_ERROR)
    {
        sync_wait(syncFd, -1);
        close(syncFd);
    }

    return result;
}

void SurfaceFlinger::renderScreenImplLocked(const RenderArea &renderArea,
                                            TraverseLayersFunction traverseLayers, bool yswap,
                                            bool useIdentityTransform)
{
    ATRACE_CALL();

    auto &engine(getRenderEngine());

    // get screen geometry
    const auto raWidth = renderArea.getWidth();
    const auto raHeight = renderArea.getHeight();

    const auto reqWidth = renderArea.getReqWidth();
    const auto reqHeight = renderArea.getReqHeight();
    Rect sourceCrop = renderArea.getSourceCrop();

    bool filtering = false;
    if (mPrimaryDisplayOrientation & DisplayState::eOrientationSwapMask)
    {
        filtering = static_cast<int32_t>(reqWidth) != raHeight ||
                    static_cast<int32_t>(reqHeight) != raWidth;
    }
    else
    {
        filtering = static_cast<int32_t>(reqWidth) != raWidth ||
                    static_cast<int32_t>(reqHeight) != raHeight;
    }

    // if a default or invalid sourceCrop is passed in, set reasonable values
    if (sourceCrop.width() == 0 || sourceCrop.height() == 0 || !sourceCrop.isValid())
    {
        sourceCrop.setLeftTop(Point(0, 0));
        sourceCrop.setRightBottom(Point(raWidth, raHeight));
    }
    else if (mPrimaryDisplayOrientation != DisplayState::eOrientationDefault)
    {
        Transform tr;
        uint32_t flags = 0x00;
        switch (mPrimaryDisplayOrientation)
        {
        case DisplayState::eOrientation90:
            flags = Transform::ROT_90;
            break;
        case DisplayState::eOrientation180:
            flags = Transform::ROT_180;
            break;
        case DisplayState::eOrientation270:
            flags = Transform::ROT_270;
            break;
        }
        tr.set(flags, raWidth, raHeight);
        sourceCrop = tr.transform(sourceCrop);
    }

    // ensure that sourceCrop is inside screen
    if (sourceCrop.left < 0)
    {
        ALOGE("Invalid crop rect: l = %d (< 0)", sourceCrop.left);
    }
    if (sourceCrop.right > raWidth)
    {
        ALOGE("Invalid crop rect: r = %d (> %d)", sourceCrop.right, raWidth);
    }
    if (sourceCrop.top < 0)
    {
        ALOGE("Invalid crop rect: t = %d (< 0)", sourceCrop.top);
    }
    if (sourceCrop.bottom > raHeight)
    {
        ALOGE("Invalid crop rect: b = %d (> %d)", sourceCrop.bottom, raHeight);
    }

    // assume ColorMode::SRGB / RenderIntent::COLORIMETRIC
    engine.setOutputDataSpace(Dataspace::SRGB);
    engine.setDisplayMaxLuminance(DisplayDevice::sDefaultMaxLumiance);

    // make sure to clear all GL error flags
    engine.checkErrors();

    Transform::orientation_flags rotation = renderArea.getRotationFlags();
    if (mPrimaryDisplayOrientation != DisplayState::eOrientationDefault)
    {
        // convert hw orientation into flag presentation
        // here inverse transform needed
        uint8_t hw_rot_90 = 0x00;
        uint8_t hw_flip_hv = 0x00;
        switch (mPrimaryDisplayOrientation)
        {
        case DisplayState::eOrientation90:
            hw_rot_90 = Transform::ROT_90;
            hw_flip_hv = Transform::ROT_180;
            break;
        case DisplayState::eOrientation180:
            hw_flip_hv = Transform::ROT_180;
            break;
        case DisplayState::eOrientation270:
            hw_rot_90 = Transform::ROT_90;
            break;
        }

        // transform flags operation
        // 1) flip H V if both have ROT_90 flag
        // 2) XOR these flags
        uint8_t rotation_rot_90 = rotation & Transform::ROT_90;
        uint8_t rotation_flip_hv = rotation & Transform::ROT_180;
        if (rotation_rot_90 & hw_rot_90)
        {
            rotation_flip_hv = (~rotation_flip_hv) & Transform::ROT_180;
        }
        rotation = static_cast<Transform::orientation_flags>((rotation_rot_90 ^ hw_rot_90) | (rotation_flip_hv ^ hw_flip_hv));
    }

    // set-up our viewport
    engine.setViewportAndProjection(reqWidth, reqHeight, sourceCrop, raHeight, yswap,
                                    rotation);
    engine.disableTexturing();

    const float alpha = RenderArea::getCaptureFillValue(renderArea.getCaptureFill());
    // redraw the screen entirely...
    engine.clearWithColor(0, 0, 0, alpha);

    traverseLayers([&](Layer *layer) {
        if (filtering)
            layer->setFiltering(true);
        layer->draw(renderArea, useIdentityTransform);
        if (filtering)
            layer->setFiltering(false);
    });
}

status_t SurfaceFlinger::captureScreenImplLocked(const RenderArea &renderArea,
                                                 TraverseLayersFunction traverseLayers,
                                                 ANativeWindowBuffer *buffer,
                                                 bool useIdentityTransform, bool forSystem,
                                                 int *outSyncFd, bool &outCapturedSecureLayers)
{
    ATRACE_CALL();

    traverseLayers([&](Layer *layer) {
        outCapturedSecureLayers =
            outCapturedSecureLayers || (layer->isVisible() && layer->isSecure());
    });

    // We allow the system server to take screenshots of secure layers for
    // use in situations like the Screen-rotation animation and place
    // the impetus on WindowManager to not persist them.
    if (outCapturedSecureLayers && !forSystem)
    {
        ALOGW("FB is protected: PERMISSION_DENIED");
        return PERMISSION_DENIED;
    }

    // this binds the given EGLImage as a framebuffer for the
    // duration of this scope.
    RE::BindNativeBufferAsFramebuffer bufferBond(getRenderEngine(), buffer);
    if (bufferBond.getStatus() != NO_ERROR)
    {
        ALOGE("got ANWB binding error while taking screenshot");
        return INVALID_OPERATION;
    }

    // this will in fact render into our dequeued buffer
    // via an FBO, which means we didn't have to create
    // an EGLSurface and therefore we're not
    // dependent on the context's EGLConfig.
    renderScreenImplLocked(renderArea, traverseLayers, true, useIdentityTransform);

    if (DEBUG_SCREENSHOTS)
    {
        getRenderEngine().finish();
        *outSyncFd = -1;

        const auto reqWidth = renderArea.getReqWidth();
        const auto reqHeight = renderArea.getReqHeight();

        uint32_t *pixels = new uint32_t[reqWidth * reqHeight];
        getRenderEngine().readPixels(0, 0, reqWidth, reqHeight, pixels);
        checkScreenshot(reqWidth, reqHeight, reqWidth, pixels, traverseLayers);
        delete[] pixels;
    }
    else
    {
        base::unique_fd syncFd = getRenderEngine().flush();
        if (syncFd < 0)
        {
            getRenderEngine().finish();
        }
        *outSyncFd = syncFd.release();
    }

    return NO_ERROR;
}

void SurfaceFlinger::checkScreenshot(size_t w, size_t s, size_t h, void const *vaddr,
                                     TraverseLayersFunction traverseLayers)
{
    if (DEBUG_SCREENSHOTS)
    {
        for (size_t y = 0; y < h; y++)
        {
            uint32_t const *p = (uint32_t const *)vaddr + y * s;
            for (size_t x = 0; x < w; x++)
            {
                if (p[x] != 0xFF000000)
                    return;
            }
        }
        ALOGE("*** we just took a black screenshot ***");

        size_t i = 0;
        traverseLayers([&](Layer *layer) {
            const Layer::State &state(layer->getDrawingState());
            ALOGE("%c index=%zu, name=%s, layerStack=%d, z=%d, visible=%d, flags=%x, alpha=%.3f",
                  layer->isVisible() ? '+' : '-', i, layer->getName().string(),
                  layer->getLayerStack(), state.z, layer->isVisible(), state.flags,
                  static_cast<float>(state.color.a));
            i++;
        });
    }
}

// ---------------------------------------------------------------------------

void SurfaceFlinger::State::traverseInZOrder(const LayerVector::Visitor &visitor) const
{
    layersSortedByZ.traverseInZOrder(stateSet, visitor);
}

void SurfaceFlinger::State::traverseInReverseZOrder(const LayerVector::Visitor &visitor) const
{
    layersSortedByZ.traverseInReverseZOrder(stateSet, visitor);
}

void SurfaceFlinger::traverseLayersInDisplay(const sp<const DisplayDevice> &hw, int32_t minLayerZ,
                                             int32_t maxLayerZ,
                                             const LayerVector::Visitor &visitor)
{
    // We loop through the first level of layers without traversing,
    // as we need to interpret min/max layer Z in the top level Z space.
    for (const auto &layer : mDrawingState.layersSortedByZ)
    {
        if (!layer->belongsToDisplay(hw->getLayerStack(), false))
        {
            continue;
        }
        const Layer::State &state(layer->getDrawingState());
        // relative layers are traversed in Layer::traverseInZOrder
        if (state.zOrderRelativeOf != nullptr || state.z < minLayerZ || state.z > maxLayerZ)
        {
            continue;
        }
        layer->traverseInZOrder(LayerVector::StateSet::Drawing, [&](Layer *layer) {
            if (!layer->belongsToDisplay(hw->getLayerStack(), false))
            {
                return;
            }
            if (!layer->isVisible())
            {
                return;
            }
            visitor(layer);
        });
    }
}

}; // namespace android

#if defined(__gl_h_)
#error "don't include gl/gl.h in this file"
#endif

#if defined(__gl2_h_)
#error "don't include gl2/gl2.h in this file"
#endif
