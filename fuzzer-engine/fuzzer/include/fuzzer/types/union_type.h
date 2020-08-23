#ifndef UNION_TYPE_H
#define UNION_TYPE_H

#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/utils/util.h>
#include <json/json.h>
#include <vector>
using namespace std;

#define UNION_INFO_DIR FUZZER_PATH "model/union/"

extern map<string, Json::Value> unionInfo;

extern void loadUnionInfo(char *unionInfoDir);

class UnionType {

public:
  UnionType(ParcelReaderWriter *parentParcelReaderWriter, string varName,
            string varType)
      : parentParcelReaderWriter(parentParcelReaderWriter), varName(varName),
        varType(varType) {
    Json::Value uInfo = unionInfo[varType];
    initUnion(uInfo);
  }

  void generateRaw(vector<uint8_t> *rawData);

private:
  ParcelReaderWriter *parentParcelReaderWriter;
  string varName;
  string varType;
  Json::Value info;
  Json::Value variable;
  Json::Value loop;
  Json::Value constraint;
  void initUnion(const Json::Value &uInfo);
};

#endif // UNION_TYPE_H