#ifndef FILE_DESCRIPTOR_H
#define FILE_DESCRIPTOR_H

#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/types/base_type.h>
#include <fuzzer/utils/util.h>
#include <json/json.h>
#include <vector>
using namespace std;

#define FILE_SEED_PATH FUZZER_PATH "seed/files/"
#define MEDIA_FILE_SEED_PATH FILE_SEED_PATH "media/"
#define APK_FILE_SEED_PATH FILE_SEED_PATH "apk/"
#define MISC_FILE_SEED_PATH FILE_SEED_PATH "misc/"
class FDType : public BaseType<int32_t> {

public:
  FDType(string varName, string varType)
      : BaseType<int32_t>(varName, varType) {}
  int32_t generate();
  int32_t generateRandomFD(map<string, int> &FDPool);
};

extern map<string, int> apkFDPool;
extern map<string, int> mediaFDPool;
extern map<string, int> miscFDPool;

extern void initFDPool();

#endif // FILE_DESCRIPTOR_H