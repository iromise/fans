#ifndef FUZZER_UTIL_H
#define FUZZER_UTIL_H
#include <dirent.h>
#include <fstream>
#include <fuzzer/utils/log.h>
#include <iostream>
#include <json/json.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
using namespace std;

#define FUZZER_PATH "/data/fuzzer/"
extern void loadJsonInfo(const char *infoDir, map<string, Json::Value> &info);
#endif // FUZZER_UTIL_H