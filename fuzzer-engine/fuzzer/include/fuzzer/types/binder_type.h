#ifndef BINDER_TYPE_H
#define BINDER_TYPE_H
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <json/json.h>

class BinderType {

public:
  BinderType(string varName, string varType, string interfaceName)
      : varName(varName), varType(varType), interfaceName(interfaceName) {}
  sp<IBinder> generate();

private:
  string varName;
  string varType;
  string interfaceName;
};
#endif // BINDER_TYPE_H
