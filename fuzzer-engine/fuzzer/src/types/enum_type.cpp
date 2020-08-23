#include <dirent.h>
#include <fstream>
#include <fuzzer/types/enum_type.h>
#include <fuzzer/types/types.h>
#include <fuzzer/utils/log.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;
map<string, vector<int64_t>> enumInfo;

int64_t randomEnum(string varType) {
  FUZZER_LOGD("Generaing enum type: %s.", varType.c_str());

  if (enumInfo.count(varType) == 0) {
    // for those typedef enum..
    if (enumInfo.count("enum " + varType) != 0) {
      varType = "enum " + varType;
    } else {
      FUZZER_LOGE("Unexpected enum %s meeted.", varType.c_str());
      exit(0);
    }
  }
  int32_t size = enumInfo[varType].size();
  if (size == 0) {
    return randomUInt64(0, UINT64_MAX);
  } else {
    int64_t idx = randomUInt64(0, size - 1);
    return enumInfo[varType][idx];
  }
}

void initEnumInfo(char *enumInfoDir) {
  struct stat s;
  lstat(enumInfoDir, &s);
  if (!S_ISDIR(s.st_mode)) {
    FUZZER_LOGE("%s is not a valid directory!", enumInfoDir);
    exit(0);
  }
  dirent *filename;
  DIR *dir;
  dir = opendir(enumInfoDir);
  if (NULL == dir) {
    FUZZER_LOGE("Can not open dir %s!", enumInfoDir);
    exit(0);
  }
  FUZZER_LOGI("Successfully opened %s.", enumInfoDir);
  int64_t value;
  while ((filename = readdir(dir)) != NULL) {
    FUZZER_LOGD("filename: %s", filename->d_name);
    if (strcmp(filename->d_name, ".") == 0 ||
        strcmp(filename->d_name, "..") == 0 ||
        strcmp(filename->d_name, "enum minikin::Bidi") == 0) {
      continue;
    } else {
      string filepath = enumInfoDir;
      string enumType = filename->d_name;
      filepath += enumType;
      ifstream ifs(filepath.c_str());
      string promotionType;
      getline(ifs, promotionType);
      if (promotionType == "int32_t" || promotionType == "int" ||
          promotionType == "std::int32_t") {
        varTypeMap[enumType] = INT32_TYPE;
      } else if (promotionType == "unsigned int" ||
                 promotionType == "uint32_t" || promotionType == "size_t") {
        // TODO: promotionType is size_t, it is very interestring..
        varTypeMap[enumType] = UINT32_TYPE;
      } else if (promotionType == "uint64_t" ||
                 promotionType == "unsigned long long" ||
                 promotionType == "__u64" || promotionType == "unsigned long") {
        varTypeMap[enumType] = UINT64_TYPE;
      } else if (promotionType == "int64_t" || promotionType == "long long" ||
                 promotionType == "long") {
        varTypeMap[enumType] = INT64_TYPE;
      } else if (promotionType.find("dependent") != string::npos) {
        varTypeMap[enumType] = INT64_TYPE;
      } else if (promotionType == "uint8_t" ||
                 promotionType == "unsigned char") {
        varTypeMap[enumType] = UINT8_TYPE;
      } else if (promotionType == "char") {
        varTypeMap[enumType] = INT8_TYPE;
      } else if (promotionType == "uint16_t") {
        varTypeMap[enumType] = UINT16_TYPE;
      } else if (promotionType == "android::vintf::RuntimeInfo::FetchFlags") {
        varTypeMap[enumType] = UINT32_TYPE;
      } else if (promotionType == "google::protobuf::internal::AtomicWord") {
        varTypeMap[enumType] = INT32_TYPE;
      } else {
        FUZZER_LOGE("PromotionType %s is unknown..", promotionType.c_str());
        continue;
        // exit(0);
      }
      int32_t dependent = 0;
      while (!ifs.eof()) {
        ifs >> value;
        if (promotionType.find("dependent") != string::npos) {
          enumInfo[enumType].push_back(dependent);
          dependent += 1;
        } else {
          enumInfo[enumType].push_back(value);
        }
      }
      ifs.close();
    }
  }
}
