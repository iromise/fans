#include <fuzzer/types/union_type.h>

#include <fstream>
#include <fuzzer/constraint_checker.h>
#include <fuzzer/dependency_solver.h>
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/types/structure_type.h>
#include <fuzzer/types/types.h>
#include <fuzzer/utils/log.h>
#include <fuzzer/utils/random.h>
#include <fuzzer/utils/util.h>
#include <string>

map<string, Json::Value> unionInfo;

void UnionType::initUnion(const Json::Value &uInfo) {
  info = uInfo["possibility"][0];
  variable = uInfo["variable"];
  loop = uInfo["loop"];
  constraint = uInfo["constraint"];
}

void UnionType::generateRaw(vector<uint8_t> *rawData) {
  FUZZER_LOGD("Random generate raw union.");
  ParcelReaderWriter parcelReaderWriter(info["data"], variable, loop,
                                        constraint);
  parcelReaderWriter.initRawStructWrite(parentParcelReaderWriter->data,
                                        rawData);
  uint32_t varIdx = randomUInt64(0, info["data"].size() - 1);
  parcelReaderWriter.readWriteItem(varIdx);
}

void loadUnionInfo(char *unionInfoDir) {
  FUZZER_LOGI("----------------------- Start loading union info. "
              "-----------------------")
  loadJsonInfo(unionInfoDir, unionInfo);
  for (auto &item : unionInfo) {
    varTypeMap[item.first] = UNION_TYPE;
  }

  FUZZER_LOGI("----------------------- Finish loading union info. "
              "-----------------------\n");
}