
#include <fuzzer/constraint_checker.h>
#include <fuzzer/parcel_writer.h>
#include <fuzzer/types/types.h>
#include <string.h>

template <typename T> T ParcelWriter::generate(BaseType<T> *type) {
  const vector<Json::Value> &selfConstraint =
      parcelReaderWriter->selectOneSelfConstraint(variable);

  // check if there exists euqal self constraint which most time should
  // be satisfied.
  ConstraintChecker checker(parcelReaderWriter);
  T value;
  if (checker.getValueFromSpecialSelfConstraint(selfConstraint, value)) {
    FUZZER_LOGD("Generating through constraint equal value.");
    parcelReaderWriter->storeValue(varName, value);
  } else {
    // generate through semantic.
    uint32_t count = checker.getTryCount(selfConstraint);
    FUZZER_LOGD("We will try to generate the variable %s with %d times.",
                varName.c_str(), count);
    uint32_t idx = 0;
    spaceNum += 2;
    while (idx < count) {
      FUZZER_LOGD("Continue trying, %u < %u", idx, count);
      value = type->generate();
      parcelReaderWriter->storeValue(varName, value);
      // check Constraint.
      if (checker.checkConstraintSet(selfConstraint) == true) {
        break;
      }
      ++idx;
    }
    spaceNum -= 2;
  }
  return value;
}

void ParcelWriter::writeRawToData(const void *buffer, size_t len) {
  vector<uint8_t> *rawData = parcelReaderWriter->rawData;
  int32_t oldSize = rawData->size();
  rawData->resize(oldSize + len);
  strncpy((char *)rawData->data() + oldSize, (char *)buffer, len);
  FUZZER_LOGD("Current target buffer size: %d", (int)rawData->size());
}

void ParcelWriter::writeInt8() {
  IntType<int8_t> intType(varName, varType);
  int8_t value = intType.generate();
  if (isRaw) {
    // only raw mode
    writeRawToData((void *)&value, sizeof(int8_t));
  } else {
    FUZZER_LOGE("This operation should only exist in raw mode.");
    exit(0);
  }
}
void ParcelWriter::writeUint8() {
  IntType<uint8_t> intType(varName, varType);
  uint8_t value = intType.generate();
  if (isRaw) {
    // only raw mode
    writeRawToData((void *)&value, sizeof(uint8_t));
  } else {
    FUZZER_LOGE("This operation should only exist in raw mode.");
    exit(0);
  }
}

void ParcelWriter::writeInt16() {
  IntType<int16_t> intType(varName, varType);
  int16_t value = intType.generate();
  if (isRaw) {
    // only raw mode
    writeRawToData((void *)&value, sizeof(int16_t));
  } else {
    FUZZER_LOGE("This operation should only exist in raw mode.");
    exit(0);
  }
}
void ParcelWriter::writeUint16() {
  IntType<uint16_t> intType(varName, varType);
  uint16_t value = intType.generate();
  if (isRaw) {
    writeRawToData((void *)&value, sizeof(uint16_t));
  } else {
    FUZZER_LOGE("This operation should only exist in raw mode.");
    exit(0);
  }
}

void ParcelWriter::writeInt32() {
  IntType<int32_t> intType(varName, varType);
  int32_t value = generate(&intType);
  if (isRaw) {
    writeRawToData((void *)&value, sizeof(int32_t));
  } else {
    parcelReaderWriter->data->writeInt32(value);
  }
  FUZZER_LOGD("Generate int32: %d.", value);
}

void ParcelWriter::writeUint32() {
  IntType<uint32_t> intType(varName, varType);
  uint32_t value = generate(&intType);

  if (isRaw) {
    writeRawToData((void *)&value, sizeof(uint32_t));
  } else {
    parcelReaderWriter->data->writeUint32(value);
  }
  FUZZER_LOGD("Generate uint32: %u.", value);
}
void ParcelWriter::writeInt64() {
  IntType<int64_t> intType(varName, varType);
  int64_t value = generate(&intType);
  if (isRaw) {
    writeRawToData((void *)&value, sizeof(int64_t));
  } else {
    parcelReaderWriter->data->writeInt64(value);
  }
  FUZZER_LOGD("Generate int64: %ld.", value);
}
void ParcelWriter::writeUint64() {
  IntType<uint64_t> intType(varName, varType);
  uint64_t value = generate(&intType);
  if (isRaw) {
    writeRawToData((void *)&value, sizeof(uint64_t));
  } else {
    parcelReaderWriter->data->writeUint64(value);
  }
  FUZZER_LOGD("Generate uint64: %lu.", value);
}

void ParcelWriter::writeFloat() {
  FloatType floatType(varName, varType);
  float value = generate(&floatType);
  if (isRaw) {
    writeRawToData((void *)&value, sizeof(float));
  } else {
    parcelReaderWriter->data->writeFloat(value);
  }
  FUZZER_LOGD("Generate float: %f.", value);
}
void ParcelWriter::writeDouble() {
  DoubleType doubleType(varName, varType);
  double value = generate(&doubleType);
  if (isRaw) {
    writeRawToData((void *)&value, sizeof(double));
  } else {
    parcelReaderWriter->data->writeDouble(value);
  }
  FUZZER_LOGD("Generate double: %f.", value);
}
void ParcelWriter::writeBool() {
  BoolType boolType(varName, varType);
  bool value = generate(&boolType);
  if (isRaw) {
    uint32_t tmp = value;
    writeRawToData((void *)&tmp, sizeof(uint32_t));
  } else {
    if (variable.isMember("flattenable")) {
      // here we should just write one byte
      uint8_t tmp = randomUInt64(0, 255);
      memcpy((void *)parcelReaderWriter->data->data(), &tmp, 1);
      // seems very hack...
      parcelReaderWriter->data->setDataPosition(
          parcelReaderWriter->data->dataPosition() + 1);
    } else {
      parcelReaderWriter->data->writeBool(value);
    }
  }
  FUZZER_LOGD("Generate bool: %d.", (int)value);
}

void ParcelWriter::writeString16() {
  string value;
  if (varName == "ServiceInterfaceDescriptor") { // this is very special.
    value = parcelReaderWriter->interfaceToken;
    String16 token(value.c_str());
    if (isRaw) {
      FUZZER_LOGE("This operation shouldn't exist in raw mode.");
      exit(0);
    } else {
      parcelReaderWriter->data->writeInterfaceToken(token);
    }
    parcelReaderWriter->storeValue(varName, parcelReaderWriter->interfaceToken);
  } else {
    StringType stringType(varName, varType);
    value = generate(&stringType);
    if (isRaw) {
      FUZZER_LOGE("This operation shouldn't exist in raw mode.");
      exit(0);
    } else {
      parcelReaderWriter->data->writeString16(String16(value.c_str()));
    }
  }
  FUZZER_LOGD("Generate String16: %s.", value.c_str());
}
void ParcelWriter::writeString8() {
  StringType stringType(varName, varType);
  string value = generate(&stringType);
  if (isRaw) {
    FUZZER_LOGE("This operation shouldn't exist in raw mode.");
    exit(0);
  } else {
    parcelReaderWriter->data->writeString8(String8(value.c_str()));
  }
  FUZZER_LOGD("Generate String8: %s.", value.c_str());
}
void ParcelWriter::writeCString() {
  StringType stringType(varName, varType);
  string value = generate(&stringType);
  if (isRaw) {
    FUZZER_LOGE("This operation shouldn't exist in raw mode.");
    exit(0);
  } else {
    if (variable.isMember("utf8") == true &&
        variable["utf8"].asBool() == true) {
      parcelReaderWriter->data->writeUtf8AsUtf16(value);
    } else {
      parcelReaderWriter->data->writeCString(value.c_str());
    }
  }
  FUZZER_LOGD("Generate CString: %s.", value.c_str());
}
void ParcelWriter::writeFileDescriptor() {
  if (isRaw) {
    FUZZER_LOGE("This operation shouldn't exist in raw mode.");
    exit(0);
  } else if (parcelReaderWriter->flattenableLevel > 0) {
    // just count the number of FileDescriptors, do not write.
    parcelReaderWriter->fdCount += 1;
    FUZZER_LOGD("Count file descriptor: %u.", parcelReaderWriter->fdCount);
  } else {
    FDType fdType(varName, varType);
    int32_t fd = generate(&fdType);
    bool takeOwnership = variable["takeOwnership"].asBool();
    parcelReaderWriter->data->writeFileDescriptor(fd, takeOwnership);
    FUZZER_LOGD("Generate file descriptor: %u.", fd);
  }
}

void ParcelWriter::writeStrongBinder() {
  BinderType binderType(varName, varType, variable["interfaceName"].asString());
  sp<IBinder> binder = binderType.generate();
  if (binder == NULL) {
    FUZZER_LOGD("Binder is NULL.");
  }
  if (isRaw) {
    FUZZER_LOGE("This operation shouldn't exist in raw mode.");
    exit(0);
  } else {
    parcelReaderWriter->data->writeStrongBinder(binder);
  }
}

void ParcelWriter::writeParcelStructure() {
  StructureType structureType(parcelReaderWriter, varName, varType);
  if (variable.isMember("have_parcelable") &&
      variable["have_parcelable"].asInt() == 1) {
    FUZZER_LOGD(
        "This parcel related struct is deserialized by readParcelable.");
    parcelReaderWriter->data->writeInt32(1);
  }
  structureType.generate();
  parcelReaderWriter->eraseStructStatus(varType);
}

void ParcelWriter::writeFlattenableStructure() {
  StructureType structureType(parcelReaderWriter, varName, varType);
  structureType.generateFlattenable();
  parcelReaderWriter->eraseStructStatus(varType);
}

void ParcelWriter::writeLightFlattenableStructure() {
  StructureType structureType(parcelReaderWriter, varName, varType);
  structureType.generateLightFlattenable();
  parcelReaderWriter->eraseStructStatus(varType);
}

void ParcelWriter::writeRawStructure() {
  StructureType structureType(parcelReaderWriter, varName, varType);
  vector<uint8_t> rawData;
  structureType.generateRaw(&rawData);
  if (isRaw) {
    // this means we are now inside a raw structure..
    // we should write it to the rawbuffer...
    writeRawToData((void *)rawData.data(), rawData.size());
  } else {
    parcelReaderWriter->data->write(rawData.data(), rawData.size());
  }
  parcelReaderWriter->eraseStructStatus(varType);
}

void ParcelWriter::writeUnion() {
  UnionType unionType(parcelReaderWriter, varName, varType);
  vector<uint8_t> rawData;
  unionType.generateRaw(&rawData);
  if (isRaw) {
    // this means we are now inside a raw structure..
    // we should write it to the rawbuffer...
    writeRawToData((void *)rawData.data(), rawData.size());
  } else {
    parcelReaderWriter->data->write(rawData.data(), rawData.size());
  }
}

void ParcelWriter::writeFunction() {
  FunctionType functionType(parcelReaderWriter, variable);
  functionType.generate();
  parcelReaderWriter->eraseStructStatus(variable["func_name"].asString());
}

void ParcelWriter::writeArray() {
  ArrayType arrayType(parcelReaderWriter, variable);
  if (isRaw) {
    // this means we are now inside a raw structure..
    // we should write it to the rawbuffer...
    vector<uint8_t> rawData;
    arrayType.generateRaw(&rawData);
    writeRawToData((void *)rawData.data(), rawData.size());
  } else {
    arrayType.generate();
  }
}
void ParcelWriter::writeBlob() {
  BlobType blobType(parcelReaderWriter, variable);
  if (isRaw) {
    FUZZER_LOGE("Unexpected raw meeted in writeBlob.");
    exit(0);
  } else {
    blobType.generate();
  }
}
void ParcelWriter::generateManually(VarType varTypeEnum) {
  FUZZER_LOGI("Generate variable manually.");
  spaceNum += 2;
  switch (varTypeEnum) {
  case INT8_TYPE: {
    writeInt8();
    break;
  }
  case UINT8_TYPE: {
    writeUint8();
    break;
  }
  case INT16_TYPE: {
    writeInt16();
    break;
  }
  case UINT16_TYPE: {
    writeUint16();
    break;
  }
  case INT32_TYPE: {
    writeInt32();
    break;
  }
  case UINT32_TYPE: {
    writeUint32();
    break;
  }
  case INT64_TYPE: {
    writeInt64();
    break;
  }
  case UINT64_TYPE: {
    writeUint64();
    break;
  }
  case STRING16_TYPE: {
    writeString16();
    break;
  }
  case STRING8_TYPE: {
    writeString8();
    break;
  }
  case CSTRING_TYPE: {
    writeCString();
    break;
  }
  case FILE_DESCRIPTOR_TYPE: {
    writeFileDescriptor();
    break;
  }
  case FLOAT_TYPE: {
    writeFloat();
    break;
  }
  case DOUBLE_TYPE: {
    writeDouble();
    break;
  }
  case BOOL_TYPE: {
    writeBool();
    break;
  }
  case PARCEL_STRUCTURE_TYPE: {
    writeParcelStructure();
    break;
  }
  case FLATTENABLE_STRUCTURE_TYPE: {
    writeFlattenableStructure();
    break;
  }
  case LIGHT_FLATTENABLE_STRUCTURE_TYPE: {
    writeLightFlattenableStructure();
    break;
  }
  case RAW_STRUCTURE_TYPE: {
    writeRawStructure();
    break;
  }
  case UNION_TYPE: {
    writeUnion();
    break;
  }
  case STRONG_BINDER_TYPE: {
    writeStrongBinder();
    break;
  }
  case ARRAY_TYPE: {
    writeArray();
    break;
  }
  case FUNCTION_TYPE: {
    writeFunction();
    break;
  }
  case BLOB_TYPE: {
    writeBlob();
    break;
  }
  case INTEGER_LITERAL: {
    // TODO: integer literal may be large than int.
    parcelReaderWriter->data->writeInt32(variable["value"].asInt());
    break;
    // FUZZER_LOGE("generating data Parcel item, varName: %s, type: %s, isRaw:
    // "
    //             "%d",
    //             varName.c_str(), varType.c_str(), isRaw);
    // FUZZER_LOGE("can not exists integer literal..");
    // exit(0);
  }
  }
  spaceNum -= 2;
}

void ParcelWriter::write() {
  FUZZER_LOGI("Start generating variable, varName: %s, type: %s, isRaw: "
              "%d, dataPosition: %06d.",
              varName.c_str(), varType.c_str(), isRaw,
              (int)parcelReaderWriter->data->dataPosition());
  // before generating a variable, we need to clear its cached status.
  parcelReaderWriter->isVarCached[varName] = false;

  VarType varTypeEnum = getVarTypeEnum(varType);
  const Json::Value &dependencySet = variable["dependency"];
  IntType<int> intType("", "");
  if (dependencySet.size() > 0 && intType.nOutOf(7, 10)) {
    if (varTypeEnum == STRONG_BINDER_TYPE) {
      DependencySolver solver(NULL, BINDER_DEPENDENCY, dependencySet);
      if (solver.canUseDependency()) {
        solver.solve();
        parcelReaderWriter->data->writeStrongBinder(solver.binder);
      } else {
        generateManually(varTypeEnum);
      }
    } else {
      DependencySolver solver(parcelReaderWriter->data, COMMON_DEPENDENCY,
                              dependencySet);
      if (solver.canUseDependency()) {
        solver.solve();
      } else {
        generateManually(varTypeEnum);
      }
    }
  } else {
    generateManually(varTypeEnum);
  }
  parcelReaderWriter->resetSelfConstraints(varName);
  FUZZER_LOGI("Finish generating variable, varName: %s, type: %s, isRaw: "
              "%d, dataPosition: %06d.",
              varName.c_str(), varType.c_str(), isRaw,
              (int)parcelReaderWriter->data->dataPosition());
}