#ifndef ARRAY_TYPE_H
#define ARRAY_TYPE_H
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/types/int_type.h>
#include <fuzzer/utils/random.h>
#include <fuzzer/utils/util.h>
#include <json/json.h>
#include <vector>
using namespace std;

class ArrayType {
public:
  ArrayType(ParcelReaderWriter *parcelReaderWriter, const Json::Value &varInfo)
      : parcelReaderWriter(parcelReaderWriter), variable(varInfo) {
    varName = variable["name"].asString();
    varType = variable["type"].asString();
  }

  void generate();
  void generateRaw(vector<uint8_t> *rawData);
  void read();

private:
  ParcelReaderWriter *parcelReaderWriter;
  const Json::Value &variable;
  string varName;
  string varType;
  uint32_t varSize;
};

#endif // ARRAY_TYPE_H