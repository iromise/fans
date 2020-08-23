#include <fstream>
#include <fuzzer/constraint_checker.h>
#include <fuzzer/dependency_solver.h>
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/types/function_type.h>
#include <fuzzer/types/types.h>
#include <fuzzer/utils/log.h>
#include <fuzzer/utils/random.h>
#include <fuzzer/utils/util.h>
#include <string>

map<string, Json::Value> functionInfo;

void FunctionType::initArgv(ParcelReaderWriter *parcelReaderWriter) {
  Json::Value parentArg, arg;
  string parentArgName, parentArgType, argName, argType;
  FUZZER_LOGD("Start initing function argv.")
  for (uint32_t i = 0; i < parentArgv.size(); ++i) {
    FUZZER_LOGD("Current argv idx %d.", i);
    parentArg = parentArgv[i];
    parentArgName = parentArg["name"].asString();
    parentArgType = parentArg["type"].asString();
    arg = argv[i];
    argName = arg["name"].asString();
    argType = arg["type"].asString();
    if (parentArgType.find("android::Parcel") != string::npos) {
      FUZZER_LOGD("We do not init android::Parcel from parent argv.");
      continue;
    } else if (parentArgType.find("*") != string::npos) {
      FUZZER_LOGD("We do not init pointer from parent argv.");
      continue;
    } else if (parentArgType.find("<") != string::npos) {
      FUZZER_LOGD("We do not init strong pointer or vector from parent argv.");
      continue;
    } else if (parentArgType == "android::status_t") {
      FUZZER_LOGD("We do not init android::status_t from parent argv.");
      continue;
    } else if (parentArgType == "_Bool") {
      bool value = parentParcelReaderWriter->getVarValue(parentArgName);
      parcelReaderWriter->storeValue(argName, value);
    } else if (parentArgType == "int") {
      int value = parentParcelReaderWriter->getVarValue(parentArgName);
      parcelReaderWriter->storeValue(argName, value);
    } else if (parentArgType ==
               "struct android::hardware::hidl_vec<unsigned char>") {
      FUZZER_LOGD(
          "Now we do not consider struct android::hardware::hidl_vec<unsigned "
          "char> when initing arguments.");
    } else {
      FUZZER_LOGE("Variable type %s is not supported now in initArgv.",
                  parentArgType.c_str());
      exit(0);
    }
  }
  if (parentArgv.size() < argv.size()) {
    FUZZER_LOGI("Function %s has default value.", funcName.c_str());
    if (funcName == "keystore::readKeymasterBlob") {
      // this function's second argv can be default value, i.e., true
      argName = argv[1]["name"].asString();
      parcelReaderWriter->storeValue(argName, true);
    } else {
      FUZZER_LOGE("Function %s is not well supported for default value.",
                  funcName.c_str());
      exit(0);
    }
  }
  FUZZER_LOGD("Finish initing function argv.")
}

void FunctionType::generate() {
  ParcelReaderWriter parcelReaderWriter(info["data"], variable, loop,
                                        constraint);
  parcelReaderWriter.initFunctionWrite(parentParcelReaderWriter->data);
  initArgv(&parcelReaderWriter);
  parcelReaderWriter.start();
}

void FunctionType::read() {
  ParcelReaderWriter parcelReaderWriter(info["reply"], variable, loop,
                                        constraint);
  parcelReaderWriter.initFunctionRead(parentParcelReaderWriter->reply);
  initArgv(&parcelReaderWriter);
  if (parentParcelReaderWriter->isDependency) {
    parcelReaderWriter.isDependency = parentParcelReaderWriter->isDependency;
    parcelReaderWriter.targetParcel = parentParcelReaderWriter->targetParcel;
  }
  parcelReaderWriter.start();
}

void loadFunctionInfo(char *functionInfoDir) {
  FUZZER_LOGI("----------------------- Start loading function info. "
              "-----------------------");
  loadJsonInfo(functionInfoDir, functionInfo);
  FUZZER_LOGI("----------------------- Finish loading function info. "
              "-----------------------\n");
}