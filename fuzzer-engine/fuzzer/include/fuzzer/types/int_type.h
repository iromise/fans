#ifndef INT_TYPE_H
#define INT_TYPE_H
#include <arpa/inet.h>
#include <binder/Parcel.h>
#include <ctime>
#include <fuzzer/constraint_checker.h>
#include <fuzzer/dependency_solver.h>
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/types/base_type.h>
#include <fuzzer/types/enum_type.h>
#include <fuzzer/types/int_type.h>
#include <fuzzer/utils/log.h>
#include <fuzzer/utils/random.h>
#include <fuzzer/utils/util.h>
#include <iostream>
#include <json/json.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <random>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <fuzzer/interface/camera.h>

using namespace std;

// Some potentially interesting integers.
#define specIntsLen 42
extern const uint64_t specialInts[specIntsLen];

/* Not part of public API */
// copy from
// http://androidxref.com/9.0.0_r3/xref/system/media/audio/include/system/audio.h#182
static inline uint32_t
my_audio_channel_mask_from_representation_and_bits(uint32_t representation,
                                                   uint32_t bits) {
  return ((representation << 30) | bits);
}

template <typename T> class IntType : public BaseType<T> {

  uint64_t customRandomUInt64() {
    uint64_t value = randomUInt64(0, UINT64_MAX);
    if (nOutOf(100, 182)) {
      value %= 10;
    } else if (nOutOf(50, 82)) {
      value = specialInts[randomUInt64(0, specIntsLen - 1)];
    } else if (nOutOf(10, 32)) {
      value %= 256;
    } else if (nOutOf(10, 22)) {
      value %= 4 << 10;
    } else if (nOutOf(10, 12)) {
      value %= 64 << 10;
    } else {
      value %= 1 << 31;
    }

    // second stage
    if (nOutOf(40, 107)) {
      value = -value;
    } else {
      value <<= randomUInt64(0, 63);
    }
    return value;
  }
  bool isPid(string varName, string varType) {
    if (this->varType == "pid_t") {
      return true;
    } else if (this->varName == "pid" || this->varName == "tid" ||
               this->varName.find("Pid") != string::npos) {
      return true;
    } else {
      return false;
    }
  }
  bool isUid(string varName, string varType) {
    transform(varName.begin(), varName.end(), varName.begin(), ::tolower);
    if (varType == "uid_t") {
      return true;
    } else if (varName.find("uid") != string::npos) {
      return true;
    } else {
      return false;
    }
  }
  bool isUserId(string varName) {
    transform(varName.begin(), varName.end(), varName.begin(), ::tolower);
    if (varName.find("userid") != string::npos) {
      return true;
    } else {
      return false;
    }
  }

public:
  IntType(string varName, string varType) : BaseType<T>(varName, varType) {}

  T generate() {
    IntType<int32_t> intType("", "");
    if (intType.nOutOf(7, 10)) {
      // consider specific semantic meaning
      // is enumerate type?
      if (enumInfo.count(this->varType) != 0) {
        if (this->varType == "enum android::DrmPlugin::SecurityLevel") {
          value = 1; // kSecurityLevelMax
        } else {
          value = randomEnum(this->varType);
        }
      } else if (this->varName == "as") {
        value = randomEnum("enum audio_source_t");
      } else if (this->varName == "vs") {
        value = randomEnum("enum android::video_source");
      } else if (this->varName == "of") {
        value = randomEnum("enum android::output_format");
      } else if (this->varName == "ae") {
        value = randomEnum("enum android::audio_encoder");
      } else if (this->varName == "ve") {
        value = randomEnum("enum android::video_encoder");
      } else if (this->varName == "in_event") {
        value = randomEnum("enum android::AudioSystem::sync_event_t");
      } else if (
          this->varName == "in_format" ||
          // http://androidxref.com/9.0.0_r3/xref/frameworks/av/media/libstagefright/FrameDecoder.cpp#154
          this->varName == "colorFormat") {
        // http://androidxref.com/9.0.0_r3/xref/frameworks/av/camera/aidl/android/hardware/camera2/ICameraDeviceUser.aidl#105
        // TODO: in_format in KeystoreService

        value = randomEnum("enum android_pixel_format_t");
      } else if (this->varName == "cmdCode") {
        // http://androidxref.com/9.0.0_r3/xref/frameworks/av/media/libaudioclient/IEffect.cpp#158

        // http://androidxref.com/9.0.0_r3/xref/frameworks/av/services/audioflinger/Effects.cpp#1647
        value = randomEnum("enum effect_command_e");
      } else if (this->varName == "stateFlags") {
        value = randomEnum(
            "enum android::ISurfaceComposer::(anonymous at "
            "frameworks.native.libs.gui.include.gui.ISurfaceComposer.h:61:5)");
      } else if (this->varName == "keyCode") {
        value = randomEnum("enum android::(anonymous at "
                           "frameworks.av.media.libmedia.include.media."
                           "mediametadataretriever.h:35:1)");
      } else if (this->varName == "audio_devices_t") {
        value =
            randomEnum("enum (anonymous at "
                       "system.media.audio.include.system.audio-base.h:289:1)");
      } else if (isPid(this->varName, this->varType)) {
        if (this->varName == "in_clientPid") {
          value = -1;
        } else {
          value = generatePid();
        }
      } else if (isUid(this->varName, this->varType)) {
        if (this->varName == "in_clientUid") {
          value = -1;
        } else {
          value = generateUid();
        }
      } else if (this->varName == "in_cameraId") {
        int32_t cameraNum = Camera::getNumberOfCameras();
        value = randomUInt64(0, cameraNum - 1);
      } else if (this->varName == "in_templateId") {
        // http://androidxref.com/9.0.0_r3/xref/frameworks/av/camera/aidl/android/hardware/camera2/ICameraDeviceUser.aidl#123
        value = randomUInt64(1, 6);
      } else if (this->varName == "in_halVersion") {
        int32_t tmp = randomUInt64(0, 1);
        if (tmp == 0) {
          value = -1;
        } else {
          // http://androidxref.com/9.0.0_r3/xref/frameworks/av/services/camera/libcameraservice/CameraService.cpp#634
          value = (T)256;
        }
      } else if (this->varName == "in_eventId") {
        value = randomUInt64(0, 1);
      } else if (this->varName == "in_apiVersion") {
        value = randomUInt64(1, 2);
      } else if (this->varName == "sensor") {
        value = generateSensor();
      } else if (this->varName.find("mode") != string::npos) {
        // http://androidxref.com/9.0.0_r3/xref/frameworks/av/include/camera/android/hardware/ICamera.h#48

        // http://androidxref.com/9.0.0_r3/xref/frameworks/native/libs/gui/ISurfaceComposer.cpp#809

        // http://androidxref.com/9.0.0_r3/xref/system/netd/server/binder/android/net/INetd.aidl#551
        value = randomUInt64(0, 3);
      } else if (this->varName == "callback_flag") {
        // http://androidxref.com/9.0.0_r3/xref/system/core/include/system/camera.h#60
        value = randomUInt64(0, 7);
      } else if (this->varName == "msgType") {
        // http://androidxref.com/9.0.0_r3/xref/frameworks/av/camera/ICamera.cpp#447
        // http://androidxref.com/9.0.0_r3/xref/system/core/include/system/camera.h#72
        value = generateMsgType();
      } else if (this->varName == "command") {
        // http://androidxref.com/9.0.0_r3/xref/frameworks/av/camera/ICamera.cpp#467
        // http://androidxref.com/9.0.0_r3/xref/frameworks/av/services/camera/libcameraservice/api1/Camera2Client.cpp#1595
        value = randomUInt64(1, 11);
      } else if (this->varName == "arg1") {
        value = randomUInt64(0, 1);
      } else if (this->varType == "audio_channel_mask_t") {
        // http://androidxref.com/9.0.0_r3/xref/system/media/audio/include/system/audio.h#137
        value = generateChannelMask();
      } else if (this->varName == "format") {
        // http://androidxref.com/9.0.0_r3/xref/frameworks/native/services/sensorservice/SensorService.cpp#1080
        value = 1;
      } else if (this->varName == "type") {
        value = randomUInt64(1, 2);
      } else if (isSize(this->varName)) {
        value = generateSize();
      } else if (this->varName.find("__kernel_sa_family_t") != string::npos) {
        value = generateSinFamily();
      } else if (isSocketPort()) {
        value = generateSocketPort();
      } else if (this->varName.find("in_addr_t") != string::npos) {
        value = generateSocketAddr();
      } else if (this->varName == "amt") {
        int32_t flag = randomUInt64(0, 5);
        if (flag > 0) {
          return sizeof(struct sockaddr_in);
        } else {
          return 0;
        }
      } else if (this->varName ==
                     "aaudio::AAudioStreamConfiguration::DeviceId" ||
                 this->varName ==
                     "aaudio::AAudioStreamConfiguration::BufferCapacity") {
        // http://androidxref.com/9.0.0_r3/xref/frameworks/av/media/libaaudio/src/core/AAudioStreamParameters.cpp#99
        value = randomUInt64(0, INT32_MAX);
      } else if (this->varName ==
                 "aaudio::AAudioStreamConfiguration::SampleRate") {
        value = randomUInt64(7999, 1600000);
        if (value == 79999) {
          value = 0;
        }
      } else if (this->varName ==
                 "aaudio::AAudioStreamConfiguration::SamplesPerFrame") {
        // http://androidxref.com/9.0.0_r3/xref/frameworks/av/media/libaaudio/src/core/AAudioStreamParameters.cpp#52
        value = randomUInt64(0, 8);
      } else if (this->varType == "sound_trigger_module_handle_t") {
        // http://androidxref.com/9.0.0_r3/xref/frameworks/av/soundtrigger/ISoundTriggerHwService.cpp#43
        value = randomUInt64(1, 2);
      } else if (this->varName == "length") {
        value = specialInts[randomUInt64(0, specIntsLen - 1)];
      } else if (this->varName == "offset") {
        if (intType.nOutOf(8, 10)) {
          value = 0;
        } else {
          value = specialInts[randomUInt64(0, specIntsLen - 1)];
        }
      } else if (this->varName == "code") {
        // special for code in IAppOpsService.cpp
        // http://androidxref.com/9.0.0_r3/xref/frameworks/base/services/core/java/com/android/server/AppOpsService.java#1867
        // http://androidxref.com/9.0.0_r3/xref/frameworks/base/core/java/android/app/AppOpsManager.java#356
        // should provide a better method.
        value = randomUInt64(0, 100);
      } else if (this->varName == "idx") {
        // for track idx in IMediaExtractor
        if (intType.nOutOf(8, 10)) {
          value = 0;
        } else {
          value = randomUInt64(1, 3);
        }
      } else if (this->varName == "id") {
        if (intType.nOutOf(5, 10)) {
          value = randomUInt64(0, 2);
        } else {
          value = customRandomUInt64();
        }
      } else if (this->varName == "GraphicBufferHeader") {
        if (intType.nOutOf(2, 3)) {
          value = (T)'GB01';
        } else {
          value = (T)'GBFR';
        }
      } else if (this->varName == "infoType") {
        // http://androidxref.com/9.0.0_r3/xref/frameworks/av/drm/libdrmframework/plugins/passthru/src/DrmPassthruPlugIn.cpp#80

        // http://androidxref.com/9.0.0_r3/xref/frameworks/av/include/drm/DrmInfoRequest.h#33
        value = randomUInt64(1, 4);
      } else if (isSdkVersion(this->varName)) {
        value = randomUInt64(1, 30);
      } else if (isUserId(this->varName)) {
        value = randomUInt64(0, 10);
      } else if (this->varName == "in_keyType") {
        if (intType.nOutOf(1, 2)) {
          value = (T)6;
        } else {
          value = (T)408;
        }
      } else if (this->varName.find("family") != string::npos) {
        if (IntType<int>::nOutOf(1, 2)) {
          value = 4;
        } else {
          value = 6;
        }
      } else if (this->varName.find("which") != string::npos) {
        if (IntType<int>::nOutOf(1, 2)) {
          value = 1;
        } else {
          value = 2;
        }
      } else if (this->varName.find("level") != string::npos) {
        value = randomUInt64(0, 10);
      } else if (this->varName.find("TruncBits") != string::npos ||
                 this->varName.find("IcvBits") != string::npos) {
        value = randomUInt64(0, 128);
      } else if (this->varName.find("eventType") != string::npos) {
        value = randomUInt64(1, 2);
      } else if (this->varName.find("pullCode") != string::npos) {
        // http://androidxref.com/9.0.0_r3/xref/frameworks/base/cmds/statsd/src/atoms.proto#132
        value = randomUInt64(10000, 10021);
      } else if (this->varName == "validTypes") {
        value = randomUInt64(0, 3);
      } else {
        // random int;
        value = customRandomUInt64();
      }
    } else {
      value = customRandomUInt64();
    }

    return value;
  }
  bool isNetId(string varName) {
    transform(varName.begin(), varName.end(), varName.begin(), ::tolower);
    if (varName.find("netid") != string::npos) {
      return true;
    } else if (varName.find("networkIds") != string::npos) {
      return true;
    } else {
      return false;
    }
  }
  uint64_t generateNetId() {
    int n = randomUInt64(0, 1);
    // http://androidxref.com/9.0.0_r3/xref/system/netd/server/NetworkController.cpp#53
    if (n == 0) {
      return randomUInt64(100, 65535);
    } else {
      return randomUInt64(1, 50);
    }
  }
  bool isSdkVersion(string varName) {
    transform(varName.begin(), varName.end(), varName.begin(), ::tolower);
    if (varName.find("sdkversion") != string::npos) {
      return true;
    }
    return false;
  }
  bool isSocketPort() {
    if (this->varType == "__be16" &&
        this->varName.find("port") != string::npos) {
      return true;
    } else {
      return false;
    }
  }
  unsigned short generateSocketPort() { return randomUInt64(0, 65535); }
  unsigned int generateSocketAddr() {
    string ip = to_string(randomUInt64(0, 255));
    for (int i = 0; i < 3; ++i) {
      ip += "." + to_string(randomUInt64(0, 255));
    }
    return inet_addr(ip.c_str());
  }
  unsigned short generateSinFamily() {
    int32_t flag = randomUInt64(0, 9);
    if (flag > 5) {
      return AF_INET;
    } else if (flag > 2) {
      return AF_INET6;
    } else {
      return randomUInt64(0, 45);
    }
  }

  bool isSize(string varName) {
    transform(varName.begin(), varName.end(), varName.begin(), ::tolower);
    if (varName.find("size") != string::npos ||
        varName.find("num") != string::npos ||
        varName.find("argc") != string::npos ||
        // special for _aidl_data.resizeOutVector
        varName.find("vector_size") != string::npos) {
      return true;
    } else {
      return false;
    }
  }

  static uint64_t generateSize() {
    if (IntType<int32_t>::nOutOf(5, 100)) {
      return -randomUInt64(0, 10);
    } else {
      return specialInts[randomUInt64(0, 31)];
    }
  }
  static int32_t generatePid() {
    uint32_t flag = randomUInt64(0, 9);
    int32_t value;
    if (flag < 8) {
      value = randomUInt64(0, 32768 - 1 - 1);
    } else {
      value = getpid();
    }
    return value;
  }
  static int32_t generateUid() {
    // uid = userId * 100000  + appId //single user: uid = appId
    // userId = uid / 100000
    // appId = uid % 100000

    // suppose max 10 users
    return randomUInt64(0, 10 * 100000 + 100000 - 1);
  }

  uint32_t generateChannelMask() {
    // represent can be 0 or 2
    uint32_t represent = randomUInt64(0, 1);
    if (represent == 1) {
      represent += 1;
    }
    // TODO: how to generate bits in a better way.
    uint32_t bits = 4;
    return my_audio_channel_mask_from_representation_and_bits(represent, bits);
  }
  int32_t generateSensor() {
    // http://androidxref.com/9.0.0_r3/xref/frameworks/base/core/java/android/os/BatteryStats.java#906
    // TODO: we should generate the sensor in a better way.
    int32_t r = randomUInt64(0, 4);
    if (r == 0) {
      return -1000;
    } else {
      return randomUInt64(-10000, 10000);
    }
  }
  int32_t generateMsgType() {
    int32_t r = randomUInt64(0, 12);
    if (r < 12) {
      return 1 << r;
    } else {
      return 0xFFFF;
    }
  }

  /**
   * @brief random generate v, check if v is in [0, n-1], n/outOf
   *
   * @param n
   * @param outOf
   * @return true, if value in [0, n - 1]
   * @return false, if value in [n, outOf-1]
   */
  static bool nOutOf(uint64_t n, uint64_t outOf) {
    if (n >= outOf) {
      // log here.
      FUZZER_LOGE("bad probability");
      exit(0);
    }
    uint64_t v = randomUInt64(0, outOf - 1);
    return v < n;
  }

  T value;
};

#endif // INT_TYPE_H
