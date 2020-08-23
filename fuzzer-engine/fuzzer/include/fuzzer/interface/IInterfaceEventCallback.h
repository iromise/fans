#ifndef IINTERFACE_EVENT_CALLBACK_H
#define IINTERFACE_EVENT_CALLBACK_H
#include <binder/IInterface.h>

#include <android/net/wifi/IInterfaceEventCallback.h>

#include <android/net/wifi/BnInterfaceEventCallback.h>
#include <android/net/wifi/BpInterfaceEventCallback.h>

#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
using namespace android;

extern sp<IBinder> generateIInterfaceEventCallback();
#endif // IINTERFACE_EVENT_CALLBACK_H