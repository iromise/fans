#include <fuzzer/interface/IMediaHTTPService.h>

#include <android_util_Binder.h>
#include <fuzzer/utils/java_vm.h>
#include <media/stagefright/foundation/ADebug.h>
#include <nativehelper/ScopedLocalRef.h>

sp<IMediaHTTPService> MyCreateHTTPServiceInCurrentJavaContext() {
  ScopedLocalRef<jclass> clazz(
      env, env->FindClass("android/media/MediaHTTPService"));
  CHECK(clazz.get() != NULL);

  jmethodID constructID = env->GetMethodID(clazz.get(), "<init>", "()V");
  CHECK(constructID != NULL);

  ScopedLocalRef<jobject> httpServiceObj(
      env, env->NewObject(clazz.get(), constructID));

  sp<IMediaHTTPService> httpService;
  if (httpServiceObj.get() != NULL) {
    jmethodID asBinderID =
        env->GetMethodID(clazz.get(), "asBinder", "()Landroid/os/IBinder;");
    CHECK(asBinderID != NULL);

    ScopedLocalRef<jobject> httpServiceBinderObj(
        env, env->CallObjectMethod(httpServiceObj.get(), asBinderID));
    CHECK(httpServiceBinderObj.get() != NULL);

    sp<IBinder> binder = ibinderForJavaObject(env, httpServiceBinderObj.get());

    httpService = interface_cast<IMediaHTTPService>(binder);
  }

  return httpService;
}

sp<IBinder> generateIMediaHTTPService() {
  sp<IMediaHTTPService> mediaHTTPService =
      MyCreateHTTPServiceInCurrentJavaContext();
  return IInterface::asBinder(mediaHTTPService);
}