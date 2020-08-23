#include <fuzzer/interface/IDrmClient.h>

// static bool gGotVendorDefinedEvent = false;

// typedef Vector<uint8_t> idvec_t;

// struct DrmListener : virtual public BnDrmClient {
// private:
//   AMediaDrm *mObj;
//   AMediaDrmEventListener mListener;

// public:
//   DrmListener(AMediaDrm *obj, AMediaDrmEventListener listener)
//       : mObj(obj), mListener(listener) {}
//   void notify(DrmPlugin::EventType eventType, int extra, const Parcel *obj);
// };

// struct AMediaDrm {
//   sp<IDrm> mDrm;
//   sp<IDrmClient> mDrmClient;
//   List<idvec_t> mIds;
//   KeyedVector<String8, String8> mQueryResults;
//   Vector<uint8_t> mKeyRequest;
//   Vector<uint8_t> mProvisionRequest;
//   String8 mProvisionUrl;
//   String8 mPropertyString;
//   Vector<uint8_t> mPropertyByteArray;
//   List<Vector<uint8_t>> mSecureStops;
//   sp<DrmListener> mListener;
// };

// void DrmListener::notify(DrmPlugin::EventType eventType, int extra,
//                          const Parcel *obj) {
//   if (!mListener) {
//     return;
//   }

//   AMediaDrmSessionId sessionId = {NULL, 0};
//   int32_t sessionIdSize = obj->readInt32();
//   if (sessionIdSize) {
//     uint8_t *sessionIdData = new uint8_t[sessionIdSize];
//     sessionId.ptr = sessionIdData;
//     sessionId.length = sessionIdSize;
//     obj->read(sessionIdData, sessionId.length);
//   }

//   int32_t dataSize = obj->readInt32();
//   uint8_t *data = NULL;
//   if (dataSize) {
//     data = new uint8_t[dataSize];
//     obj->read(data, dataSize);
//   }

//   // translate DrmPlugin event types into their NDK equivalents
//   AMediaDrmEventType ndkEventType;
//   switch (eventType) {
//   case DrmPlugin::kDrmPluginEventProvisionRequired:
//     ndkEventType = EVENT_PROVISION_REQUIRED;
//     break;
//   case DrmPlugin::kDrmPluginEventKeyNeeded:
//     ndkEventType = EVENT_KEY_REQUIRED;
//     break;
//   case DrmPlugin::kDrmPluginEventKeyExpired:
//     ndkEventType = EVENT_KEY_EXPIRED;
//     break;
//   case DrmPlugin::kDrmPluginEventVendorDefined:
//     ndkEventType = EVENT_VENDOR_DEFINED;
//     break;
//   default:
//     ALOGE("Invalid event DrmPlugin::EventType %d, ignored", (int)eventType);
//     goto cleanup;
//   }

//   (*mListener)(mObj, &sessionId, ndkEventType, extra, data, dataSize);

// cleanup:
//   delete[] sessionId.ptr;
//   delete[] data;
// }

class MyDrmListener : virtual public BnDrmClient {
public:
  MyDrmListener(){};
  void notify(DrmPlugin::EventType eventType, int extra, const Parcel *obj);
};
void MyDrmListener::notify(DrmPlugin::EventType eventType, int extra,
                           const Parcel *obj) {
  return;
}

sp<IBinder> generateIDrmClient() {
  // AMediaDrm *mObj = new AMediaDrm();
  // AMediaDrmEventListener listener = NULL;
  // return NULL;
  sp<MyDrmListener> drmListener = new MyDrmListener();
  return IInterface::asBinder(drmListener);
}