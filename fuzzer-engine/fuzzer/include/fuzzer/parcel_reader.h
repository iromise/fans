#ifndef PARCEL_READER_H
#define PARCEL_READER_H

#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/types/types.h>
#include <json/json.h>

using namespace std;
using namespace android;
class ParcelReader {
public:
  ParcelReader(ParcelReaderWriter *parcelReaderWriter, const Json::Value &info)
      : parcelReaderWriter(parcelReaderWriter), variable(info) {
    varName = variable["name"].asString();
    varType = variable["type"].asString();
    isRaw = parcelReaderWriter->getIsRaw();
  }
  void read();

private:
  ParcelReaderWriter *parcelReaderWriter;
  const Json::Value &variable;
  string varName;
  string varType;
  bool isRaw = false;

  void readRawFromReply(void *buffer, size_t len);
  void writeRawToData(void *buffer, size_t len);
  void checkIsRaw();

  void readInt8();
  void readUint8();

  void readInt16();
  void readUint16();

  void readInt32();

  void readUint32();
  void readIntegerLiteral();

  void readInt64();

  void readUint64();

  void readString16();

  void readString8();

  void readCString();

  void readFileDescriptor();
  void readFloat();
  void readDouble();

  void readBool();

  void readBlob();
  void readParcelStructure();
  void readFlattenableStructure();
  void readLightFlattenableStructure();
  void readRawStructure();

  void readUnion();

  void readStrongBinder();
  void readArray();
  void readFunction();
};

#endif // PARCEL_READER_H