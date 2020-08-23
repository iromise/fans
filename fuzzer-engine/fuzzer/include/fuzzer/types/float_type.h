#ifndef FLOAT_TYPE_H
#define FLOAT_TYPE_H
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/types/base_type.h>
#include <json/json.h>

class FloatType : public BaseType<float> {

public:
  FloatType(string varName, string varType)
      : BaseType<float>(varName, varType) {}
  float generate();
};
#endif // FLOAT_TYPE_H
