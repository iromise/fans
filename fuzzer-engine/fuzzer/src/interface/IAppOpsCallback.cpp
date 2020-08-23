#include <fuzzer/interface/IAppOpsCallback.h>
#include <fuzzer/interface/ICamera.h>

// #include <CameraService.h>
#include <fuzzer/types/types.h>

// class MyBasicClient : public CameraService::BasicClient {
// public:
//   AppOpsManager mAppOpsManager;
//   MyBasicClient(const sp<CameraService> &cameraService,
//                 const sp<IBinder> &remoteCallback,
//                 const String16 &clientPackageName, const String8
//                 &cameraIdStr, int cameraFacing, int clientPid, uid_t
//                 clientUid, int servicePid)
//       : CameraService::BasicClient(cameraService,
//                                    IInterface::asBinder(cameraClient),
//                                    clientPackageName, cameraIdStr,
//                                    cameraFacing, clientPid, clientUid,
//                                    servicePid) {}
//   void opChanged(int32_t op, const String16 &packageName) {
//     ATRACE_CALL();

//     String8 name(packageName);
//     String8 myName(mClientPackageName);

//     if (op != AppOpsManager::OP_CAMERA) {
//       ALOGW("Unexpected app ops notification received: %d", op);
//       return;
//     }

//     int32_t res;
//     res = mAppOpsManager.checkOp(AppOpsManager::OP_CAMERA, mClientUid,
//                                  mClientPackageName);
//     ALOGV("checkOp returns: %d, %s ", res,
//           res == AppOpsManager::MODE_ALLOWED
//               ? "ALLOWED"
//               : res == AppOpsManager::MODE_IGNORED
//                     ? "IGNORED"
//                     : res == AppOpsManager::MODE_ERRORED ? "ERRORED"
//                                                          : "UNKNOWN");

//     if (res != AppOpsManager::MODE_ALLOWED) {
//       ALOGI("Camera %s: Access for \"%s\" revoked", mCameraIdStr.string(),
//             myName.string());
//       block();
//     }
//   }

//   class OpsCallback : public BnAppOpsCallback {
//   public:
//     explicit OpsCallback(wp<MyBasicClient> client);
//     virtual void opChanged(int32_t op, const String16 &packageName);

//   private:
//     wp<MyBasicClient> mClient;

//   }; // class OpsCallback
// };   // class BasicClient

// MyBasicClient::OpsCallback::OpsCallback(wp<MyBasicClient> client)
//     : mClient(client) {}

// void MyBasicClient::OpsCallback::opChanged(int32_t op,
//                                            const String16 &packageName) {
//   sp<MyBasicClient> client = mClient.promote();
//   if (client != NULL) {
//     client->opChanged(op, packageName);
//   }
// }

// sp<IBinder> generateIAppOpsCallback() {
//   //   sp<IServiceManager> sm = defaultServiceManager();
//   //   sp<IBinder> binder = sm->checkService(String16("media.camera"));
//   //   if (binder == NULL) {
//   //     return NULL;
//   //   }

//   sp<CameraService> cameraService = new CameraService();
//   sp<IBinder> cameraClient = generateCamera();

//   StringType stringType("cameraIdStr", "String8");
//   String8 cameraIdStr(stringType.generate().c_str());

//   IntType<int> intType("cameraFacing", "int");
//   sp<MyBasicClient> client(cameraService, cameraClient,
//                            StringType::generatePackageName(), cameraIdStr,
//                            intType.generate(), IntType::generatePid(),
//                            IntType::generateUid(), IntType::generatePid());

// }

sp<IBinder> generateIAppOpsCallback() { return NULL; }