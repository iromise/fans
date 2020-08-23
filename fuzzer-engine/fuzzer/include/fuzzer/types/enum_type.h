#ifndef ENUMERATION_TYPE_H
#define ENUMERATION_TYPE_H

#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/utils/util.h>
#include <json/json.h>
#include <vector>
using namespace std;

#define ENUMERATION_INFO_DIR FUZZER_PATH "model/enumeration/"

extern map<string, vector<int64_t>> enumInfo;

extern int64_t randomEnum(string varType);
extern void initEnumInfo(char *enumInfoDir);
#endif // ENUMERATION_TYPE_H
