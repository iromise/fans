#ifndef DOUBLE_TYPE_H
#define DOUBLE_TYPE_H
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/types/base_type.h>
#include <json/json.h>
class DoubleType : public BaseType<double> {

public:
  DoubleType(string varName, string varType)
      : BaseType<double>(varName, varType) {}
  double generate();
};

#endif // DOUBLE_TYPE_H
