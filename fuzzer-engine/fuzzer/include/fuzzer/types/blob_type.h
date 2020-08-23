#ifndef BLOB_TYPE_H
#define BLOB_TYPE_H
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/types/int_type.h>
#include <fuzzer/utils/random.h>
#include <fuzzer/utils/util.h>
#include <json/json.h>
#include <vector>
using namespace std;

class BlobType {
public:
  BlobType(ParcelReaderWriter *parcelReaderWriter, const Json::Value &varInfo)
      : parcelReaderWriter(parcelReaderWriter), variable(varInfo) {
    blobSize = this->parcelReaderWriter->getVarSize(variable);
  }

  void generate();
  void read();

private:
  ParcelReaderWriter *parcelReaderWriter;
  const Json::Value &variable;
  uint64_t blobSize;
};

#endif // BLOB_TYPE_H