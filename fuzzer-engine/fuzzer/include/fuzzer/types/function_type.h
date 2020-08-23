
#ifndef FUNCTION_TYPE_H
#define FUNCTION_TYPE_H

#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/utils/random.h>
#include <fuzzer/utils/util.h>
#include <json/json.h>
#include <vector>
using namespace std;

#define FUNCTION_INFO_DIR FUZZER_PATH "model/function/"

extern map<string, Json::Value> functionInfo;

extern void loadFunctionInfo(char *functionInfoDir);

class FunctionType {

public:
  FunctionType(ParcelReaderWriter *parentParcelReaderWriter,
               const Json::Value &varInfo)
      : parentParcelReaderWriter(parentParcelReaderWriter) {

    funcName = varInfo["func_name"].asString();
    signature = varInfo["signature"].asString();
    parentArgv = varInfo["argv"];
    FUZZER_LOGD("Function name: %s, signature: %s", funcName.c_str(),
                signature.c_str());
    string tmp = funcName + "+" + signature;
    Json::Value function;
    if (functionInfo.count(tmp) != 0) {
      function = functionInfo[tmp];
    } else {
      if (funcName == "android::IMediaPlayer::invoke") {
        uint64_t flag = randomUInt64(0, 1);
        if (flag == 0) {
          FUZZER_LOGD("Choose android::NuPlayer2Driver::invoke.");
          function = functionInfo
              ["android::NuPlayer2Driver::invoke+android::status_t (const "
               "class android::Parcel &, class android::Parcel *)"];
        } else {
          FUZZER_LOGD("Choose android::NuPlayerDriver::invoke.");
          function = functionInfo
              ["android::NuPlayerDriver::invoke+android::status_t (const class "
               "android::Parcel &, class android::Parcel *)"];
        }
      } else if (funcName == "android::IMediaPlayer::setMetadataFilter") {
        function = functionInfo
            ["android::MediaPlayerService::Client::setMetadataFilter+android::"
             "status_t (const class android::Parcel &)"];
      } else if (funcName == "android::IMediaPlayer::setParameter") {
        function = functionInfo
            ["android::MediaPlayerService::Client::setParameter+android::"
             "status_t (int, const class android::Parcel &)"];
      } else if (funcName == "android::MediaPlayerBase::setParameter") {
        uint64_t flag = randomUInt64(0, 1);
        if (flag == 0) {
          FUZZER_LOGD("Choose android::NuPlayerDriver::setParameter.");
          function =
              functionInfo["android::NuPlayerDriver::setParameter+android::"
                           "status_t (int, const class android::Parcel &)"];
        } else {
          FUZZER_LOGD("Choose android::NuPlayer2Driver::setParameter.");
          function =
              functionInfo["android::NuPlayer2Driver::setParameter+android::"
                           "status_t (int, const class android::Parcel &)"];
        }
      } else if (funcName ==
                 "android::MediaPlayerService::Client::setAudioAttributes_l") {
        function =
            functionInfo["(anonymous namespace)::unmarshallAudioAttributes"];
      } else if (funcName == "android::IMediaPlayer::getMetadata") {
        function = functionInfo
            ["android::MediaPlayerService::Client::getMetadata+android::status_"
             "t (_Bool, _Bool, class android::Parcel *)"];
      } else if (funcName == "android::MediaPlayerBase::getMetadata") {
        uint64_t flag = randomUInt64(0, 1);
        if (flag == 0) {
          FUZZER_LOGD("Choose android::NuPlayerDriver::getMetadata.");
          function = functionInfo
              ["android::NuPlayerDriver::getMetadata+android::status_t (const "
               "media::class Metadata::Filter &, class android::Parcel *)"];
        } else {
          FUZZER_LOGD("Choose android::NuPlayer2Driver::getMetadata.");
          function = functionInfo
              ["android::NuPlayer2Driver::getMetadata+android::status_t (const "
               "media::class Metadata::Filter &, class android::Parcel *)"];
        }
      } else if (funcName == "android::IMediaPlayer::getParameter") {
        function =
            functionInfo["android::MediaPlayerService::Client::getParameter+"
                         "android::status_t (int, class android::Parcel *)"];
      } else if (funcName == "android::MediaPlayerBase::getParameter") {
        uint64_t flag = randomUInt64(0, 1);
        if (flag == 0) {
          FUZZER_LOGD("Choose android::NuPlayerDriver::getParameter.");
          function =
              functionInfo["android::NuPlayerDriver::getParameter+android::"
                           "status_t (int, class android::Parcel *)"];
        } else {
          FUZZER_LOGD("Choose android::NuPlayer2Driver::getParameter.");
          function =
              functionInfo["android::NuPlayer2Driver::getParameter+android::"
                           "status_t (int, class android::Parcel *)"];
        }
      } else if (funcName == "android::IMediaPlayerService::pullBatteryData") {
        function = functionInfo
            ["android::MediaPlayerService::BatteryTracker::pullBatteryData+"
             "android::status_t (class android::Parcel *)"];
      } else if (funcName == "android::IMediaExtractor::getMetrics") {
        function =
            functionInfo["android::RemoteMediaExtractor::getMetrics+android::"
                         "status_t (class android::Parcel *)"];
      } else if (funcName == "android::IMediaRecorder::getMetrics") {
        function = functionInfo["android::StagefrightRecorder::getMetrics+"
                                "android::status_t (class android::Parcel *)"];
      } else {
        FUZZER_LOGE("Function %s is not supported.", tmp.c_str());
        exit(0);
      }
    }
    int32_t len = function["possibility"].size();
    // uint32_t possIdx = (uint32_t)randomUInt64(0, len - 1);
    // FUZZER_LOGD("Choose Function Possibility Index %u", possIdx);
    // it seems that we can not random choose the possIdx.
    // http://androidxref.com/9.0.0_r3/xref/frameworks/native/libs/gui/view/Surface.cpp#48
    // http://androidxref.com/9.0.0_r3/xref/frameworks/native/libs/gui/IGraphicBufferProducer.cpp#671
    // TODO:
    info = function["possibility"][len - 1];
    variable = function["variable"];
    loop = function["loop"];
    constraint = function["constraint"];
    argv = function["argv"];
  }

  void generate();
  void read();

private:
  ParcelReaderWriter *parentParcelReaderWriter;
  Json::Value parentArgv;
  string funcName;
  string signature;
  Json::Value info;
  Json::Value variable;
  Json::Value loop;
  Json::Value constraint;
  Json::Value argv;

  void initArgv(ParcelReaderWriter *parcelReaderWriter);
};

#endif // FUNCTION_TYPE_H