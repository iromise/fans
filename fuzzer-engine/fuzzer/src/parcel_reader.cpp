#include <assert.h>
#include <binder/Parcel.h>
#include <fuzzer/parcel_reader.h>
void ParcelReader::readRawFromReply(void *buffer, size_t len) {
  parcelReaderWriter->reply->read(buffer, len);
}
void ParcelReader::writeRawToData(void *buffer, size_t len) {
  parcelReaderWriter->targetParcel->write(buffer, len);
  FUZZER_LOGD("Current Target data position: %d.",
              (int)parcelReaderWriter->targetParcel->dataPosition());
}

void ParcelReader::readInt8() {
  FUZZER_LOGE("The program shouldn't come to here.");
  exit(0);
}
void ParcelReader::readUint8() {
  FUZZER_LOGE("The program shouldn't come to here.");
  exit(0);
}

void ParcelReader::readInt16() {
  FUZZER_LOGE("The program shouldn't come to here.");
  exit(0);
}
void ParcelReader::readUint16() {
  FUZZER_LOGE("The program shouldn't come to here.");
  exit(0);
}
void ParcelReader::checkIsRaw() {
  if (isRaw) {
    FUZZER_LOGE("isRaw can not be true.");
    exit(0);
  }
}

void ParcelReader::readInt32() {
  checkIsRaw();
  int32_t value;
  value = parcelReaderWriter->reply->readInt32();
  parcelReaderWriter->storeValue(varName, value);
  FUZZER_LOGD("read int32: %d.", value);
  // detect leak

  // if dependency, solve dependency.
  if (parcelReaderWriter->isDependency) {
    parcelReaderWriter->targetParcel->writeInt32(value);
  }
}

void ParcelReader::readUint32() {
  checkIsRaw();
  uint32_t value;

  value = parcelReaderWriter->reply->readUint32();
  parcelReaderWriter->storeValue(varName, value);
  FUZZER_LOGD("read uint32: %u.", value);
  // detect leak

  // if dependency, solve dependency.
  if (parcelReaderWriter->isDependency) {
    parcelReaderWriter->targetParcel->writeUint32(value);
  }
}
void ParcelReader::readIntegerLiteral() {
  int32_t value = parcelReaderWriter->reply->readInt32();
  FUZZER_LOGD("read IntegerLiteral: %d.", value);
}
void ParcelReader::readInt64() {
  checkIsRaw();
  int64_t value;

  value = parcelReaderWriter->reply->readInt64();
  parcelReaderWriter->storeValue(varName, value);
  FUZZER_LOGD("read int64: %ld.", value);

  // detect leak

  // if dependency, solve dependency.
  if (parcelReaderWriter->isDependency) {
    parcelReaderWriter->targetParcel->writeInt64(value);
  }
}
void ParcelReader::readUint64() {
  checkIsRaw();
  uint64_t value;
  value = parcelReaderWriter->reply->readUint64();
  parcelReaderWriter->storeValue(varName, value);
  FUZZER_LOGD("read int64: %lu.", value);
  // detect leak

  // if dependency, solve dependency.
  if (parcelReaderWriter->isDependency) {
    parcelReaderWriter->targetParcel->writeUint64(value);
  }
}

void ParcelReader::readString16() {
  checkIsRaw();
  String16 value = parcelReaderWriter->reply->readString16();
  string tmp = String8(value).string();
  FUZZER_LOGD("read String16: %s.", tmp.c_str());
  parcelReaderWriter->storeValue(varName, tmp);
  // detect leak
  // if dependency, solve dependency.
  if (parcelReaderWriter->isDependency) {
    parcelReaderWriter->targetParcel->writeString16(value);
  }
}
void ParcelReader::readString8() {
  checkIsRaw();
  String8 value = parcelReaderWriter->reply->readString8();
  string tmp = value.string();
  parcelReaderWriter->storeValue(varName, tmp);
  FUZZER_LOGD("read String8: %s.", tmp.c_str());
  // detect leak
  // if dependency, solve dependency.
  if (parcelReaderWriter->isDependency) {
    parcelReaderWriter->targetParcel->writeString8(value);
  }
}
void ParcelReader::readCString() {
  checkIsRaw();
  string value;
  if (variable.isMember("utf8") == true && variable["utf8"].asBool() == true) {
    parcelReaderWriter->reply->readUtf8FromUtf16(&value);
  } else {
    const char *readFromReply;
    readFromReply = parcelReaderWriter->reply->readCString();
    if (readFromReply == NULL) {
      value = "";
    } else {
      value = readFromReply;
    }
  }
  parcelReaderWriter->storeValue(varName, value);
  FUZZER_LOGD("read CString: %s.", value.c_str());
  // detect leak
  // if dependency, solve dependency.
  if (parcelReaderWriter->isDependency) {
    parcelReaderWriter->targetParcel->writeCString(value.c_str());
  }
}
void ParcelReader::readFileDescriptor() {
  checkIsRaw();
  int32_t fd = parcelReaderWriter->reply->readFileDescriptor();
  parcelReaderWriter->storeValue(varName, fd);
  FUZZER_LOGD("read File Descriptor: %d.", fd);
  // detect leak
  // if dependency, solve dependency.
  if (parcelReaderWriter->isDependency) {
    parcelReaderWriter->targetParcel->writeFileDescriptor(fd);
  }
}

void ParcelReader::readFloat() {
  checkIsRaw();
  float value = parcelReaderWriter->reply->readFloat();
  parcelReaderWriter->storeValue(varName, value);
  FUZZER_LOGD("read Float: %f.", value);
  // detect leak
  // if dependency, solve dependency.
  if (parcelReaderWriter->isDependency) {
    parcelReaderWriter->targetParcel->writeFloat(value);
  }
}
void ParcelReader::readDouble() {
  checkIsRaw();
  double value = parcelReaderWriter->reply->readDouble();
  parcelReaderWriter->storeValue(varName, value);
  FUZZER_LOGD("read Double: %f.", value);
  // detect leak
  // if dependency, solve dependency.
  if (parcelReaderWriter->isDependency) {
    parcelReaderWriter->targetParcel->writeDouble(value);
  }
}
void ParcelReader::readBool() {
  checkIsRaw();
  bool value;
  if (variable.isMember("flattenable")) {
    // here we should just read one byte
    memcpy(&value, parcelReaderWriter->reply->data(), 1);
    // seems very hack...
    parcelReaderWriter->reply->setDataPosition(
        parcelReaderWriter->reply->dataPosition() + 1);
  } else {
    value = parcelReaderWriter->reply->readBool();
  }
  parcelReaderWriter->storeValue(varName, value);
  FUZZER_LOGD("read Bool: %d.", (int32_t)value);
  // detect leak
  // if dependency, solve dependency.
  if (parcelReaderWriter->isDependency) {
    parcelReaderWriter->targetParcel->writeBool(value);
  }
}

void ParcelReader::readStrongBinder() {
  checkIsRaw();
  sp<IBinder> binder;
  status_t status = parcelReaderWriter->reply->readStrongBinder(&binder);
  FUZZER_LOGD("Read binder status: %d.", status);
  if (binder == NULL) {
    FUZZER_LOGD("Binder is NULL.");
  }
  if (parcelReaderWriter->isDependency) {
    parcelReaderWriter->binder = binder;
  }
}

void ParcelReader::readBlob() {
  BlobType blobType(parcelReaderWriter, variable);
  blobType.read();
}

void ParcelReader::readParcelStructure() {
  // special for binder::Status
  if (varType == "class android::binder::Status") {
    parcelReaderWriter->exceptionCode =
        parcelReaderWriter->reply->readExceptionCode();
    parcelReaderWriter->storeValue("mException", (int)0);
    FUZZER_LOGD("Exception code %d.", parcelReaderWriter->exceptionCode);
  } else {
    StructureType structureType(parcelReaderWriter, varName, varType);
    if (variable.isMember("have_parcelable") &&
        variable["have_parcelable"].asInt() == 1) {
      FUZZER_LOGD(
          "This parcel related struct is deserialized by writeParcelable.");
      int32_t present = parcelReaderWriter->reply->readInt32();
      FUZZER_LOGD("present: %d.", present);
    }
    structureType.read();
  }
}
void ParcelReader::readFlattenableStructure() {
  StructureType structureType(parcelReaderWriter, varName, varType);
  structureType.readFlattenable();
}
void ParcelReader::readLightFlattenableStructure() {
  StructureType structureType(parcelReaderWriter, varName, varType);
  structureType.readLightFlattenable();
}

void ParcelReader::readRawStructure() {
  StructureType structureType(parcelReaderWriter, varName, varType);

  // if dependency, solve dependency.
  vector<uint8_t> buffer;
  int32_t varSize = parcelReaderWriter->getVarSize(variable);
  FUZZER_LOGD("The size of variable %s is %d.", varName.c_str(), varSize);
  if (varSize == -1) {
    FUZZER_LOGE("The size of variable %s can not be -1.", varName.c_str());
    exit(0);
  }
  buffer.resize(varSize);
  // read data from reply
  parcelReaderWriter->reply->read(buffer.data(), buffer.size());
  if (parcelReaderWriter->isDependency) {
    // write date to targetparcel
    parcelReaderWriter->targetParcel->write(buffer.data(), buffer.size());
  }
}
void ParcelReader::readUnion() {
  FUZZER_LOGE("Read union is not implemented now.");
  exit(0);
}
void ParcelReader::readArray() {
  ArrayType arrayType(parcelReaderWriter, variable);
  arrayType.read();
}

void ParcelReader::readFunction() {
  FunctionType functionType(parcelReaderWriter, variable);
  functionType.read();
}

void ParcelReader::read() {
  FUZZER_LOGD("Start reading variable, varName: %s, type: %s, isRaw: "
              "%d, dataPosition: %06d",
              varName.c_str(), varType.c_str(), isRaw,
              (int)parcelReaderWriter->reply->dataPosition());

  VarType varTypeEnum = getVarTypeEnum(varType);
  switch (varTypeEnum) {
  case INT8_TYPE: {
    readInt8();
    break;
  }
  case UINT8_TYPE: {
    readUint8();
    break;
  }
  case INT16_TYPE: {
    readInt16();
    break;
  }
  case UINT16_TYPE: {
    readUint16();
    break;
  }
  case INT32_TYPE: {
    readInt32();
    break;
  }
  case UINT32_TYPE: {
    readUint32();
    break;
  }
  case INT64_TYPE: {
    readInt64();
    break;
  }
  case UINT64_TYPE: {
    readUint64();
    break;
  }
  case STRING16_TYPE: {
    readString16();
    break;
  }
  case STRING8_TYPE: {
    readString8();
    break;
  }
  case CSTRING_TYPE: {
    readCString();
    break;
  }
  case FILE_DESCRIPTOR_TYPE: {
    readFileDescriptor();
    break;
  }
  case FLOAT_TYPE: {
    readFloat();
    break;
  }
  case DOUBLE_TYPE: {
    readDouble();
    break;
  }
  case BOOL_TYPE: {
    readBool();
    break;
  }
  case PARCEL_STRUCTURE_TYPE: {
    readParcelStructure();
    break;
  }
  case FLATTENABLE_STRUCTURE_TYPE: {
    readFlattenableStructure();
    break;
  }
  case LIGHT_FLATTENABLE_STRUCTURE_TYPE: {
    readLightFlattenableStructure();
    break;
  }
  case RAW_STRUCTURE_TYPE: {
    readRawStructure();
    break;
  }
  case UNION_TYPE: {
    readUnion();
    break;
  }
  case STRONG_BINDER_TYPE: {
    readStrongBinder();
    break;
  }
  case ARRAY_TYPE: {
    readArray();
    break;
  }
  case FUNCTION_TYPE: {
    readFunction();
    break;
  }
  case BLOB_TYPE: {
    readBlob();
    break;
  }
  case INTEGER_LITERAL: {
    readIntegerLiteral();
    break;
  }
  }
  parcelReaderWriter->resetSelfConstraints(varName);
  FUZZER_LOGD("Finish reading variable, varName: %s, type: %s, isRaw: "
              "%d, dataPosition: %06d.",
              varName.c_str(), varType.c_str(), isRaw,
              (int)parcelReaderWriter->reply->dataPosition());
}