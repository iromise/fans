#ifndef FUZZER_JAVA_VM
#define FUZZER_JAVA_VM

#include <core_jni_helpers.h>
#include <dlfcn.h>
#include <jni.h>
// #include <sigchain.h>

typedef int (*JNI_CreateJavaVM_t)(void *, void *, void *);
typedef jint (*registerNatives_t)(JNIEnv *env, jclass clazz);

extern JavaVM *vm;
extern JNIEnv *env;
int init_jvm(JavaVM **p_vm, JNIEnv **p_env);

#endif // FUZZER_JAVA_VM