#ifndef NATIVE_SERVICE_FUZZER_LOG_H
#define NATIVE_SERVICE_FUZZER_LOG_H
#include <android/log.h>
#include <stdio.h>
#include <utils/Log.h>
#define FUZZER_LOG_TAG "NativeServiceFuzzer"

extern int spaceNum;

typedef enum { DEBUG_LEVEL, INFO_LEVEL, ERROR_LEVEL } LOG_LEVEL;
extern LOG_LEVEL logLevel;
#define FUZZER_LOGD(...)                                                       \
  {                                                                            \
    if (DEBUG_LEVEL >= logLevel) {                                             \
      printf("%*s", spaceNum, "");                                             \
      printf(__VA_ARGS__);                                                     \
      printf("\n");                                                            \
    }                                                                          \
  }
#define FUZZER_LOGI(...)                                                       \
  {                                                                            \
    if (INFO_LEVEL >= logLevel) {                                              \
      printf("%*s", spaceNum, "");                                             \
      printf(__VA_ARGS__);                                                     \
      printf("\n");                                                            \
    }                                                                          \
  }

#define FUZZER_LOGE(...)                                                       \
  {                                                                            \
    if (ERROR_LEVEL >= logLevel) {                                             \
      printf("%*s", spaceNum, "");                                             \
      printf("Failed at %s:%d (%s)\n", __FILE__, __LINE__, __FUNCTION__);      \
      printf("%*s", spaceNum, "");                                             \
      printf(__VA_ARGS__);                                                     \
      printf("\n");                                                            \
    }                                                                          \
  }

// #define FUZZER_LOGV(...) ((void)ALOG(LOG_VERBOSE, FUZZER_LOG_TAG,
// __VA_ARGS__)) #define FUZZER_LOGD(...) ((void)ALOG(LOG_DEBUG, FUZZER_LOG_TAG,
// __VA_ARGS__)) #define FUZZER_LOGI(...) ((void)ALOG(LOG_INFO, FUZZER_LOG_TAG,
// __VA_ARGS__))
// #define FUZZER_LOGE(...) \
//   ALOGE("Failed at %s:%d (%s)", __FILE__, __LINE__, __FUNCTION__); \
//   ((void)ALOG(LOG_ERROR, FUZZER_LOG_TAG, __VA_ARGS__))
#endif // NATIVE_SERVICE_FUZZER_LOG_H