#ifndef STRUCTURE_TYPE_H
#define STRUCTURE_TYPE_H

#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/utils/util.h>
#include <json/json.h>
#include <vector>
using namespace std;

#define STRUCUTURE_INFO_DIR FUZZER_PATH "model/structure/"

extern map<string, Json::Value> parcelableStructureDataInfo;
extern map<string, Json::Value> parcelableStructureReplyInfo;

extern map<string, Json::Value> flattenableStructureDataInfo;
extern map<string, Json::Value> flattenableStructureReplyInfo;

extern map<string, Json::Value> lightFlattenableStructureDataInfo;
extern map<string, Json::Value> lightFlattenableStructureReplyInfo;

extern map<string, Json::Value> structureRawInfo;

extern void loadStructureInfo(char *structureInfoDir);

class StructureType {

public:
  StructureType(ParcelReaderWriter *parentParcelReaderWriter, string varName,
                string varType)
      : parentParcelReaderWriter(parentParcelReaderWriter), varName(varName),
        varType(varType) {
    if (parentParcelReaderWriter->operation == WRITE_DATA) {
      if (parcelableStructureDataInfo.count(varType)) {
        structure = parcelableStructureDataInfo[varType];
      } else if (lightFlattenableStructureDataInfo.count(varType)) {
        structure = lightFlattenableStructureDataInfo[varType];
      } else if (flattenableStructureDataInfo.count(varType)) {
        structure = flattenableStructureDataInfo[varType];
      } else if (structureRawInfo.count(varType)) {
        structure = structureRawInfo[varType];
      } else {
        FUZZER_LOGE("No such type %s for variable %s.", varType.c_str(),
                    varName.c_str());
        exit(0);
      }
    } else {
      if (parcelableStructureReplyInfo.count(varType)) {
        structure = parcelableStructureReplyInfo[varType];
      } else if (lightFlattenableStructureReplyInfo.count(varType)) {
        structure = lightFlattenableStructureReplyInfo[varType];
      } else if (flattenableStructureReplyInfo.count(varType)) {
        structure = flattenableStructureReplyInfo[varType];
      } else if (structureRawInfo.count(varType)) {
        structure = structureRawInfo[varType];
      } else {
        FUZZER_LOGE("No such type %s for variable %s.", varType.c_str(),
                    varName.c_str());
        exit(0);
      }
    }
    // which possibility should we use?
    // maybe the last one.
    info = structure["possibility"][structure["possibility"].size() - 1];
    if (structureRawInfo.count(varType)) {
      // copy information from data part when dealing with raw structure.
      info["reply"] = info["data"];
    }
    variable = structure["variable"];
    loop = structure["loop"];
    constraint = structure["constraint"];
  }
  void read();
  void generate();
  void generateFlattenable();
  void readFlattenable();
  void generateLightFlattenable();
  void readLightFlattenable();
  void generateRaw(vector<uint8_t> *rawData);

private:
  ParcelReaderWriter *parentParcelReaderWriter;
  string varName;
  string varType;
  Json::Value structure;
  Json::Value info;
  Json::Value variable;
  Json::Value loop;
  Json::Value constraint;

  void initStruct();
};

#endif // STRUCTURE_TYPE_H