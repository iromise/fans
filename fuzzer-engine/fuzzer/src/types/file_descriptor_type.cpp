#include <dirent.h>
#include <fuzzer/types/file_descriptor_type.h>
#include <fuzzer/types/int_type.h>
#include <fuzzer/utils/log.h>
#include <fuzzer/utils/random.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

map<string, int> apkFDPool;
map<string, int> mediaFDPool;
map<string, int> miscFDPool;
int32_t FDType::generateRandomFD(map<string, int> &FDPool) {
  if (FDPool.size() == 0) {
    FUZZER_LOGE("The FDPool is empty.");
    exit(0);
  } else {
    map<string, int>::iterator it = FDPool.begin();
    uint64_t r = randomUInt64(0, (uint64_t)FDPool.size() - 1);
    std::advance(it, r);
    FUZZER_LOGI("Used file: %s, FD: %d.", it->first.c_str(), it->second);
    return it->second;
  }
}
int32_t FDType::generate() {
  if (varName == "in" || varName == "out" || varName == "err") {
    return generateRandomFD(miscFDPool);
  } else {
    if (IntType<int>::nOutOf(7, 10)) {
      return generateRandomFD(mediaFDPool);
    } else {
      return generateRandomFD(miscFDPool);
    }
  }
}

void _initFDPool(char *dirpath, map<string, int> &FDPool) {
  struct stat s;
  lstat(dirpath, &s);
  if (!S_ISDIR(s.st_mode)) {
    FUZZER_LOGE("%s is not a valid directory!", dirpath);
    exit(0);
  }
  dirent *filename;
  DIR *dir;
  dir = opendir(dirpath);
  if (NULL == dir) {
    FUZZER_LOGE("Can not open dir %s!", dirpath);
    exit(0);
  }
  FUZZER_LOGI("Successfully opened %s.", dirpath);
  spaceNum += 2;
  while ((filename = readdir(dir)) != NULL) {
    if (strcmp(filename->d_name, ".") == 0 ||
        strcmp(filename->d_name, "..") == 0) {
      continue;
    } else {
      string filepath = dirpath;
      filepath += filename->d_name;
      int fd = open(filepath.c_str(), O_RDWR);
      FDPool[filepath] = fd;
      FUZZER_LOGD("filepath: %s, FD: %d.", filepath.c_str(), fd);
    }
  }
  spaceNum -= 2;
  FUZZER_LOGI("");
}

void initFDPool() {
  _initFDPool((char *)MEDIA_FILE_SEED_PATH, mediaFDPool);
  _initFDPool((char *)APK_FILE_SEED_PATH, apkFDPool);
  _initFDPool((char *)MISC_FILE_SEED_PATH, miscFDPool);
}