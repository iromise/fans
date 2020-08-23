#ifndef BOOL_TYPE_H
#define BOOL_TYPE_H
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/types/base_type.h>
#include <json/json.h>

class BoolType : public BaseType<bool> {

public:
  BoolType(string varName, string varType) : BaseType<bool>(varName, varType) {}
  bool generate();
};
#endif // BOOL_TYPE_H
