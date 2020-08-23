#include <fuzzer/types/types.h>

map<string, VarType> varTypeMap;

void initVarTypeMap() {

  varTypeMap["IntegerLiteral"] = INTEGER_LITERAL;
  // int8
  varTypeMap["char"] = INT8_TYPE;
  varTypeMap["unsigned char"] = UINT8_TYPE;
  // int16
  varTypeMap["short"] = INT16_TYPE;
  varTypeMap["unsigned short"] = UINT16_TYPE;
  // int32
  varTypeMap["int"] = INT32_TYPE;
  varTypeMap["int32_t"] = INT32_TYPE;
  varTypeMap["::android::status_t"] = INT32_TYPE;

  // uint32
  varTypeMap["uint32_t"] = UINT32_TYPE;
  varTypeMap["unsigned int"] = UINT32_TYPE;

  // int64
  varTypeMap["const int64_t"] = INT64_TYPE;
  varTypeMap["int64_t"] = INT64_TYPE;
  varTypeMap["long"] = INT64_TYPE;
  varTypeMap["long long"] = INT64_TYPE;

  // uint64
  varTypeMap["uint64_t"] = UINT64_TYPE;
  varTypeMap["unsigned long"] = UINT64_TYPE;
  varTypeMap["unsigned long long"] = UINT64_TYPE;

  // float
  varTypeMap["float"] = FLOAT_TYPE;
  varTypeMap["FloatingLiteral"] = FLOAT_TYPE;
  varTypeMap["class android::half"] = FLOAT_TYPE;

  // double
  varTypeMap["double"] = DOUBLE_TYPE;

  // STRING16
  varTypeMap["android::String16"] = STRING16_TYPE;
  varTypeMap["class android::String16"] = STRING16_TYPE;
  // here we consider the following types as String16.
  varTypeMap["char16_t *"] = STRING16_TYPE;

  // STRING8
  varTypeMap["android::String8"] = STRING8_TYPE;
  varTypeMap["class android::String8"] = STRING8_TYPE;
  varTypeMap["const class android::String8"] = STRING8_TYPE;

  // STRING
  varTypeMap["string"] = CSTRING_TYPE;
  varTypeMap["std::string"] = CSTRING_TYPE;

  // when dealing with variable of these types, we consider the type of them as
  // CSTRING.
  varTypeMap["char *"] = CSTRING_TYPE;
  varTypeMap["const char *"] = CSTRING_TYPE;
  varTypeMap["unsigned char *"] = CSTRING_TYPE;

  varTypeMap["FileDescriptor"] = FILE_DESCRIPTOR_TYPE;

  // BOOL
  varTypeMap["_Bool"] = BOOL_TYPE;
  varTypeMap["bool"] = BOOL_TYPE;

  // FILE_DESCRIPTOR
  varTypeMap["FileDescriptor"] = FILE_DESCRIPTOR_TYPE;

  varTypeMap["Function"] = FUNCTION_TYPE;
  varTypeMap["class android::Parcel::Blob"] = BLOB_TYPE;

  ifstream in((char *)TYPE_MAP_PATH);
  if (!in.is_open()) {
    FUZZER_LOGE("Error opening type map file.");
    exit(0);
  }
  FUZZER_LOGI("----------------------- Start loading type map. "
              "-----------------------");
  string tmp;
  while (!in.eof()) {
    getline(in, tmp);
    if (tmp == "") {
      continue;
    }
    uint32_t plus = tmp.find("+");
    string type = tmp.substr(0, plus);
    string under = tmp.substr(plus + 1, tmp.length() - plus - 1);
    FUZZER_LOGD("type: %s, under: %s", type.c_str(), under.c_str());
    // only consider when var type is not recorded.
    if (varTypeMap.count(type) == 0) {
      if (varTypeMap.count(under) != 0) {
        varTypeMap[type] = varTypeMap[under];
      }
    }
  }
  in.close();
  FUZZER_LOGI("----------------------- Finish loading type map. "
              "-----------------------\n");
}
VarType getVarTypeEnum(string varType) {
  if (varTypeMap.count(varType) != 0) {
    return varTypeMap[varType];
  } else if (varType.find("vector<") != string::npos ||
             varType.find("Vector<") != string::npos ||
             varType.find("[") != string::npos ||
             varType.find("*") != string::npos) {
    return ARRAY_TYPE;
  } else if (varType.find("sp<") != string::npos &&
             varType.find("::I") != string::npos) {
    return STRONG_BINDER_TYPE;
  } else {
    FUZZER_LOGE("Unexpected variable type %s meeted.", varType.c_str());
    exit(0);
  }
}