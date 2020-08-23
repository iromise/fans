#include <fstream>
#include <fuzzer/constraint_checker.h>
#include <fuzzer/dependency_solver.h>
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/types/structure_type.h>
#include <fuzzer/types/types.h>
#include <fuzzer/utils/log.h>
#include <fuzzer/utils/random.h>
#include <fuzzer/utils/util.h>
#include <string>

map<string, Json::Value> parcelableStructureDataInfo;
map<string, Json::Value> parcelableStructureReplyInfo;

map<string, Json::Value> flattenableStructureDataInfo;
map<string, Json::Value> flattenableStructureReplyInfo;

map<string, Json::Value> lightFlattenableStructureDataInfo;
map<string, Json::Value> lightFlattenableStructureReplyInfo;

map<string, Json::Value> structureRawInfo;

void StructureType::generateLightFlattenable() {
  FUZZER_LOGD("Generate lightflattenable structure manually.");
  uint32_t fixed = 0;
  if (structure.isMember("fixed")) {
    fixed = structure["fixed"].asInt();
  } else {
    FUZZER_LOGE("There is no attribute fixed in type %s.", varType.c_str());
    exit(0);
  }
  uint32_t startPos = 0;

  if (!fixed) {
    startPos = parentParcelReaderWriter->data->dataPosition();
    parentParcelReaderWriter->data->writeInt32(0); // fake buf len
  }
  if (varType == "class android::FrameStats") {
    // special variable type
    int frameCount = IntType<int>::generateSize();
    parentParcelReaderWriter->storeValue("frameCount", frameCount);
  }

  ParcelReaderWriter parcelReaderWriter(info["data"], variable, loop,
                                        constraint);
  parcelReaderWriter.initLightFlattenableStructureWrite(
      parentParcelReaderWriter->data);
  parcelReaderWriter.start();

  if (!fixed) {
    // start fixing data
    parentParcelReaderWriter->data->setDataPosition(startPos);

    // fix buf len
    uint32_t currPos = parentParcelReaderWriter->data->dataPosition();
    uint32_t bufLen = currPos - startPos;
    parentParcelReaderWriter->data->writeInt32(bufLen);
    // reset data position
    parentParcelReaderWriter->data->setDataPosition(currPos);
  }
}

void StructureType::readLightFlattenable() {
  uint32_t fixed = 0;
  if (structure.isMember("fixed")) {
    fixed = structure["fixed"].asInt();
  } else {
    FUZZER_LOGE("There is not fixed attribute in type %s.", varType.c_str());
    exit(0);
  }
  uint32_t structureSize = 0;
  if (!fixed) {
    structureSize = parentParcelReaderWriter->reply->readInt32();
  }
  if (varType == "class android::FrameStats") {
    // special variable type
    // sizeof(nsecs_t)=8
    if ((structureSize - 8) % 24 != 0) {
      FUZZER_LOGE("The structuresize-8 shoule be a multuple of 3*8. Currently, "
                  "it is %u.",
                  structureSize - 8);
      exit(0);
    }
    int frameCount = (structureSize - 8) / 24;
    parentParcelReaderWriter->storeValue("frameCount", frameCount);
  }
  // Aim: read structure from tx->reply
  //      write it to target parcel when solving dependency.
  ParcelReaderWriter parcelReaderWriter(info["reply"], variable, loop,
                                        constraint);
  // default dependIdx is -1, so we do not need to set it.
  // set dependency, let ParcelReaderWriter knows that all of the structure item
  // should be read
  parcelReaderWriter.initLightFlattenableStructureRead(
      parentParcelReaderWriter->reply);
  if (parentParcelReaderWriter->isDependency) {
    parcelReaderWriter.isDependency = parentParcelReaderWriter->isDependency;
    // if another variable depends this structure, we need write it to the
    // targetParcel.
    parcelReaderWriter.targetParcel = parentParcelReaderWriter->targetParcel;
  }
  parcelReaderWriter.start();
}

void StructureType::generateFlattenable() {
  FUZZER_LOGD("Generate flattenable structure manually.");
  // increase level
  parentParcelReaderWriter->flattenableLevel += 1;

  uint32_t startPos = parentParcelReaderWriter->data->dataPosition();
  uint32_t startFdCount = parentParcelReaderWriter->fdCount;

  parentParcelReaderWriter->data->writeInt32(0); // fake buf len
  parentParcelReaderWriter->data->writeInt32(0); // fake fd count

  ParcelReaderWriter parcelReaderWriter(info["data"], variable, loop,
                                        constraint);
  parcelReaderWriter.initFlattenableStructureWrite(
      parentParcelReaderWriter->data,
      parentParcelReaderWriter->flattenableLevel);
  parcelReaderWriter.start();
  parentParcelReaderWriter->flattenableLevel =
      parcelReaderWriter.flattenableLevel;

  parentParcelReaderWriter->flattenableLevel -= 1;

  if (parentParcelReaderWriter->flattenableLevel == 0) {
    // start fixing data
    parentParcelReaderWriter->data->setDataPosition(startPos);

    // fix buf len
    uint32_t currPos = parentParcelReaderWriter->data->dataPosition();
    uint32_t bufLen = currPos - startPos;
    parentParcelReaderWriter->data->writeInt32(bufLen);

    // fix fd count
    uint32_t fdCount = parentParcelReaderWriter->fdCount - startFdCount;
    parentParcelReaderWriter->data->writeInt32(fdCount);

    // write needed file descriptors
    parentParcelReaderWriter->data->setDataPosition(currPos);
    FUZZER_LOGD("Generate %d FileDescriptors", fdCount);
    for (uint32_t i = 0; i < fdCount; ++i) {
      FDType fdType("", "");
      int32_t fd = fdType.generate();
      parentParcelReaderWriter->data->writeDupFileDescriptor(fd);
    }
  }
}

void StructureType::readFlattenable() {
  uint32_t bufLen = parentParcelReaderWriter->reply->readInt32();
  uint32_t fdCount = parentParcelReaderWriter->reply->readInt32();
  const void *readBuffer = parentParcelReaderWriter->reply->readInplace(bufLen);
  vector<int32_t> fds;
  for (uint32_t i = 0; i < fdCount; ++i) {
    fds.push_back(parentParcelReaderWriter->reply->readFileDescriptor());
  }
  if (parentParcelReaderWriter->isDependency) {
    parentParcelReaderWriter->targetParcel->writeInt32(bufLen);
    parentParcelReaderWriter->targetParcel->writeInt32(fdCount);
    void *writeBuffer =
        parentParcelReaderWriter->targetParcel->writeInplace(bufLen);
    memcpy(writeBuffer, readBuffer, bufLen);
    for (uint32_t i = 0; i < fdCount; ++i) {
      parentParcelReaderWriter->targetParcel->writeDupFileDescriptor(fds[i]);
    }
  }
}
struct uuid {
  uint32_t timeLow;
  uint16_t timeMid;
  uint16_t timeHiAndVersion;
  uint16_t clockSeq;
  uint8_t node[6];
};

int uuid_parse(const char *in, struct uuid *uuid) {
  int i;
  const char *cp;
  char buf[3];

  if (strlen(in) != 36)
    return -1;
  for (i = 0, cp = in; i <= 36; i++, cp++) {
    if ((i == 8) || (i == 13) || (i == 18) || (i == 23)) {
      if (*cp == '-')
        continue;
      else
        return -1;
    }
    if (i == 36)
      if (*cp == 0)
        continue;
    if (!isxdigit((int)*cp))
      return -1;
  }
  uuid->timeLow = strtoul(in, NULL, 16);
  uuid->timeMid = strtoul(in + 9, NULL, 16);
  uuid->timeHiAndVersion = strtoul(in + 14, NULL, 16);
  uuid->clockSeq = strtoul(in + 19, NULL, 16);
  cp = in + 24;
  buf[2] = 0;
  for (i = 0; i < 6; i++) {
    buf[0] = *cp++;
    buf[1] = *cp++;
    uuid->node[i] = strtoul(buf, NULL, 16);
  }
  return 0;
}
void StructureType::generateRaw(vector<uint8_t> *rawData) {
  FUZZER_LOGD("Generate raw structue manually.");
  if (varType == "struct audio_uuid_s") {
    // http://androidxref.com/9.0.0_r3/xref/device/google/taimen/audio_effects.xml#16
    static vector<string> audioUUIDs{
        "14804144-a5ee-4d24-aa88-0002a5d5c51b",
        "8631f300-72e2-11df-b57e-0002a5d5c51b",
        "2c4a8c24-1581-487f-94f6-0002a5d5c51b",
        "d3467faa-acc7-4d34-acaf-0002a5d5c51b",
        "1d4033c0-8557-11df-9f2d-0002a5d5c51b",
        "509a4498-561a-4bea-b3b1-0002a5d5c51b",
        "c8e70ecd-48ca-456e-8a4f-0002a5d5c51b",
        "ce772f20-847d-11df-bb17-0002a5d5c51b",
        "a0dac280-401c-11e3-9379-0002a5d5c51b",
        "48404ac9-d202-4ccc-bf84-0002a5d5c51b",
        "4a387fc0-8ab3-11df-8bad-0002a5d5c51b",
        "79a18026-18fd-4185-8233-0002a5d5c51b",
        "b707403a-a1c1-4291-9573-0002a5d5c51b",
        "c7a511a0-a3bb-11df-860e-0002a5d5c51b",
        "eb64ea04-973b-43d2-8f5e-0002a5d5c51b",
        "1b78f587-6d1c-422e-8b84-0002a5d5c51b",
        "f29a1400-a3bb-11df-8ddc-0002a5d5c51b",
        "6987be09-b142-4b41-9056-0002a5d5c51b",
        "f3e178d2-ebcb-408e-8357-0002a5d5c51b",
        "172cdf00-a3bc-11df-a72f-0002a5d5c51b",
        "aa2bebf6-47cf-4613-9bca-0002a5d5c51b",
        "1d0a1a53-7d5d-48f2-8e71-27fbd10d842c",
        "d069d9e0-8329-11df-9168-0002a5d5c51b",
        "7a8044a0-1a71-11e3-a184-0002a5d5c51b",
        "119341a0-8469-11df-81f9-0002a5d5c51b",
        "93f04452-e4fe-41cc-91f9-e475b6d1d69f",
        "fa415329-2034-4bea-b5dc-5b381c8d1e2c",
        "b4398408-1fb9-11e7-93ae-92361f002671",
        "b43988c2-1fb9-11e7-93ae-92361f002671",
        "08b8b058-0590-11e5-ac71-0025b32654a0",
        "0956df94-0590-11e5-bdbe-0025b32654a0",
        "09f303e2-0590-11e5-8fdb-0025b32654a0",
        "0ace5c08-0590-11e5-ae9e-0025b32654a0",
        "0b776dde-0590-11e5-81ba-0025b32654a0",
        "e0e6539b-1781-7261-676f-6d7573696340",
    };
    uint32_t idx = randomUInt64(0, 11);
    struct uuid tmp;
    uuid_parse(audioUUIDs[idx].c_str(), &tmp);
    parentParcelReaderWriter->data->write(&tmp, sizeof(struct uuid));
  } else {
    ParcelReaderWriter parcelReaderWriter(info["data"], variable, loop,
                                          constraint);
    parcelReaderWriter.initRawStructWrite(parentParcelReaderWriter->data,
                                          rawData);
    parcelReaderWriter.start();
  }
}
void StructureType::generate() {
  FUZZER_LOGD("Generate parcelable structure manually.");
  ParcelReaderWriter parcelReaderWriter(info["data"], variable, loop,
                                        constraint);
  parcelReaderWriter.initParcelableStructWrite(parentParcelReaderWriter->data);
  parcelReaderWriter.start();
}
void StructureType::read() {
  // Aim: read structure from tx->reply
  //      write it to target parcel when solving dependency.
  ParcelReaderWriter parcelReaderWriter(info["reply"], variable, loop,
                                        constraint);
  // default dependIdx is -1, so we do not need to set it.
  // set dependency, let ParcelReaderWrite know that all of the structure item
  // should be read
  parcelReaderWriter.initParcelableStructRead(parentParcelReaderWriter->reply);
  if (parentParcelReaderWriter->isDependency) {
    parcelReaderWriter.isDependency = parentParcelReaderWriter->isDependency;
    // if another variable depends this structure, we need write it to the
    // targetParcel.
    parcelReaderWriter.targetParcel = parentParcelReaderWriter->targetParcel;
  }
  parcelReaderWriter.start();
}

void loadStructureInfo(char *structureInfoDir) {
  FUZZER_LOGI("----------------------- Start loading structure info. "
              "-----------------------");
  // load parcelable structure
  string parcelableDir = structureInfoDir;
  string dataDir, replyDir;
  parcelableDir += "parcelable/";
  dataDir = parcelableDir + "data/";
  loadJsonInfo(dataDir.c_str(), parcelableStructureDataInfo);
  replyDir = parcelableDir + "reply/";
  loadJsonInfo(replyDir.c_str(), parcelableStructureReplyInfo);
  for (auto &item : parcelableStructureDataInfo) {
    varTypeMap[item.first] = PARCEL_STRUCTURE_TYPE;
  }
  for (auto &item : parcelableStructureReplyInfo) {
    varTypeMap[item.first] = PARCEL_STRUCTURE_TYPE;
  }
  FUZZER_LOGI("");

  // load flattenable structure
  string flattenableDir = structureInfoDir;
  flattenableDir += "flattenable/";
  dataDir = flattenableDir + "data/";
  loadJsonInfo(dataDir.c_str(), flattenableStructureDataInfo);
  replyDir = flattenableDir + "reply/";
  loadJsonInfo(replyDir.c_str(), flattenableStructureReplyInfo);
  for (auto &item : flattenableStructureDataInfo) {
    varTypeMap[item.first] = FLATTENABLE_STRUCTURE_TYPE;
  }
  for (auto &item : flattenableStructureReplyInfo) {
    varTypeMap[item.first] = FLATTENABLE_STRUCTURE_TYPE;
  }
  FUZZER_LOGI("");

  // load light_flattenable structure
  string lightFlattenableDir = structureInfoDir;
  lightFlattenableDir += "light_flattenable/";
  dataDir = lightFlattenableDir + "data/";
  loadJsonInfo(dataDir.c_str(), lightFlattenableStructureDataInfo);
  replyDir = lightFlattenableDir + "reply/";
  loadJsonInfo(replyDir.c_str(), lightFlattenableStructureReplyInfo);
  for (auto &item : lightFlattenableStructureDataInfo) {
    varTypeMap[item.first] = LIGHT_FLATTENABLE_STRUCTURE_TYPE;
  }
  for (auto &item : lightFlattenableStructureReplyInfo) {
    varTypeMap[item.first] = LIGHT_FLATTENABLE_STRUCTURE_TYPE;
  }
  FUZZER_LOGI("");

  // load raw structure
  string rawDir = structureInfoDir;
  rawDir += "raw/";
  loadJsonInfo(rawDir.c_str(), structureRawInfo);
  for (auto &item : structureRawInfo) {
    varTypeMap[item.first] = RAW_STRUCTURE_TYPE;
  }
  FUZZER_LOGI("");

  FUZZER_LOGI("----------------------- Finish loading structure info. "
              "-----------------------\n");
}