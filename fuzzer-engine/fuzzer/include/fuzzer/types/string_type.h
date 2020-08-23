#ifndef STRING_TYPE_H
#define STRING_TYPE_H
#include <binder/Parcel.h>
#include <ctime>
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/types/base_type.h>
#include <fuzzer/utils/util.h>
#include <iostream>
#include <json/json.h>
#include <random>
#include <vector>
using namespace std;

#define PACKAGE_LIST_PATH FUZZER_PATH "/seed/package_list.txt"
#define PERMISSION_LIST_PATH FUZZER_PATH "/seed/permission_list.txt"
#define MEDIA_URL_LIST_PATH FUZZER_PATH "/seed/media_url_list.txt"

extern vector<string> packageList;
extern void initPackageNameList();

extern vector<string> permissionList;
extern void initPermissionNameList();

extern vector<string> mediaUrlList;
extern void initMediaUrlList();

extern vector<string> split(const string &str, const string &pattern);

class StringType : public BaseType<string> {

  bool isPackage(string varName);
  bool isPermission(string varName);
  bool isNetworkInterfaceName(string varName);
  bool isChainName(string varName);
  bool isUrl(string varName);
  bool isPath(string varName);

public:
  StringType(string varName, string varType)
      : BaseType<string>(varName, varType) {}

  static string generatePackageName();
  static string generatePermissionName();
  static string generateNetworkInterfaceName();
  static string generateAudioParameterKeyPair();
  static string generataKeyValuePairs();
  static string generateChainName();
  static string generateIP();
  static string generateOp();
  static string generateUrl(string varName);
  static string generatePath(string varName);
  string generate();
};

#endif // STRING_TYPE_H
