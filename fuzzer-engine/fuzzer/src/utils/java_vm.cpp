#include <fuzzer/utils/java_vm.h>
#include <fuzzer/utils/log.h>
JavaVM *vm = NULL;
JNIEnv *env = NULL;
int init_jvm(JavaVM **p_vm, JNIEnv **p_env) {

  JavaVMOption opt[4];

  opt[0].optionString = "-verbose:jni";
  opt[1].optionString = "-verbose:gc";
  opt[2].optionString = "-Xcheck:jni";
  opt[3].optionString = "-Xdebug";
  JavaVMInitArgs args;
  args.version = JNI_VERSION_1_6;
  args.options = opt;
  args.nOptions = 1;
  args.ignoreUnrecognized = JNI_FALSE;

  // TODO: Should this just link against libnativehelper and use its
  // JNI_CreateJavaVM wrapper that essential does this dlopen/dlsym
  // work based on the current system default runtime?
  void *libart_dso = dlopen("libart.so", RTLD_NOW);
  void *libandroid_runtime_dso = dlopen("libandroid_runtime.so", RTLD_NOW);

  if (!libart_dso || !libandroid_runtime_dso) {
    return -1;
  }

  JNI_CreateJavaVM_t JNI_CreateJavaVM;
  JNI_CreateJavaVM = (JNI_CreateJavaVM_t)dlsym(libart_dso, "JNI_CreateJavaVM");
  if (!JNI_CreateJavaVM) {
    return -2;
  }
  registerNatives_t registerNatives;
  registerNatives = (registerNatives_t)dlsym(
      libandroid_runtime_dso,
      "Java_com_android_internal_util_WithFramework_registerNatives");
  if (!registerNatives) {
    // Attempt non-legacy version
    registerNatives = (registerNatives_t)dlsym(libandroid_runtime_dso,
                                               "registerFrameworkNatives");
    if (!registerNatives) {
      return -3;
    }
  }
  if (JNI_CreateJavaVM(&(*p_vm), &(*p_env), &args)) {
    return -4;
  }
  if (registerNatives(*p_env, 0)) {
    return -5;
  }
  FUZZER_LOGD(
      " [+] Java Virtual Machine Initialization success (vm=%p, env=%p).\n",
      *p_vm, *p_env);
  return 0;
}