#ifndef PARCEL_WRITER_H
#define PARCEL_WRITER_H

#include <fuzzer/constraint_checker.h>
#include <fuzzer/dependency_solver.h>
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/types/types.h>
#include <json/json.h>
using namespace std;
using namespace android;
class ParcelWriter {
public:
  ParcelWriter(ParcelReaderWriter *parcelReaderWriter,
               const Json::Value &variable)
      : parcelReaderWriter(parcelReaderWriter), variable(variable) {
    varName = variable["name"].asString();
    varType = variable["type"].asString();
    isRaw = parcelReaderWriter->getIsRaw();
  }
  void write();

private:
  ParcelReaderWriter *parcelReaderWriter;
  const Json::Value &variable;
  string varName;
  string varType;
  bool isRaw =
      false; // default is false, true when dealing raw structure/array.
  template <typename T> T generate(BaseType<T> *type);

  void generateManually(VarType varTypeEnum);

  void writeRawToData(const void *buffer, size_t len);

  void writeInt8();
  void writeUint8();

  void writeInt16();
  void writeUint16();

  void writeInt32();
  void writeUint32();

  void writeInt64();

  void writeUint64();

  void writeString16();

  void writeString8();

  void writeCString();

  void writeFileDescriptor();
  void writeFloat();
  void writeDouble();

  void writeBool();

  void writeParcelStructure();
  void writeFlattenableStructure();
  void writeLightFlattenableStructure();
  void writeRawStructure();

  void writeFunction();

  void writeUnion();

  void writeStrongBinder();
  void writeArray();

  void writeBlob();
};

#endif // PARCEL_WRITER_H