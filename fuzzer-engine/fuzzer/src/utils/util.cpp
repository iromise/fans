#include <fuzzer/utils/util.h>
void loadJsonInfo(const char *infoDir, map<string, Json::Value> &info) {
  Json::Value root;
  Json::Reader reader;
  struct stat s;
  lstat(infoDir, &s);
  if (!S_ISDIR(s.st_mode)) {
    FUZZER_LOGE("%s is not a valid directory!", infoDir);
    exit(0);
  }
  dirent *filename;
  DIR *dir;
  dir = opendir(infoDir);
  if (NULL == dir) {
    FUZZER_LOGE("Can not open dir %s!", infoDir);
    exit(0);
  }
  FUZZER_LOGI("Successfully opened %s.", infoDir);
  spaceNum += 2;
  while ((filename = readdir(dir)) != NULL) {
    FUZZER_LOGD("filename: %s", filename->d_name)
    if (strcmp(filename->d_name, ".") == 0 ||
        strcmp(filename->d_name, "..") == 0) {
      continue;
    } else {
      string filepath = "";
      filepath = infoDir;
      filepath += filename->d_name;
      ifstream ifs(filepath.c_str());
      reader.parse(ifs, root);
      for (Json::Value::iterator it = root.begin(); it != root.end(); ++it) {
        FUZZER_LOGD("key %s", it.key().asCString());
        info[it.key().asString()] = *it;
      }
    }
    FUZZER_LOGD("");
  }
  spaceNum -= 2;
}
