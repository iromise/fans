#ifndef TYPE_H
#define TYPE_H
#include <fuzzer/types/array_type.h>
#include <fuzzer/types/base_type.h>
#include <fuzzer/types/binder_type.h>
#include <fuzzer/types/blob_type.h>
#include <fuzzer/types/bool_type.h>
#include <fuzzer/types/double_type.h>
#include <fuzzer/types/enum_type.h>
#include <fuzzer/types/file_descriptor_type.h>
#include <fuzzer/types/float_type.h>
#include <fuzzer/types/function_type.h>
#include <fuzzer/types/int_type.h>
#include <fuzzer/types/string_type.h>
#include <fuzzer/types/structure_type.h>
#include <fuzzer/types/union_type.h>

typedef enum {
  INTEGER_LITERAL,
  UINT8_TYPE,
  INT8_TYPE,
  INT16_TYPE,
  UINT16_TYPE,
  INT32_TYPE,
  INT64_TYPE,
  UINT32_TYPE,
  UINT64_TYPE,
  FILE_DESCRIPTOR_TYPE,
  FLOAT_TYPE,
  DOUBLE_TYPE,
  STRING16_TYPE,
  STRING8_TYPE,
  CSTRING_TYPE,
  STRONG_BINDER_TYPE,
  BOOL_TYPE,
  PARCEL_STRUCTURE_TYPE,
  FLATTENABLE_STRUCTURE_TYPE,
  LIGHT_FLATTENABLE_STRUCTURE_TYPE,
  RAW_STRUCTURE_TYPE,
  UNION_TYPE,
  ARRAY_TYPE,
  FUNCTION_TYPE,
  BLOB_TYPE
} VarType;

#define TYPE_MAP_PATH FUZZER_PATH "model/typemap.txt"

// map type string to Enum varType
// e.g. typeMap["int32"]=INT
extern map<string, VarType> varTypeMap;
void initVarTypeMap();
VarType getVarTypeEnum(string varType);

#endif // TYPE_H
