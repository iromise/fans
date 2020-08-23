#include <algorithm>
#include <fstream>
#include <fuzzer/constraint_checker.h>
#include <fuzzer/types/file_descriptor_type.h>
#include <fuzzer/types/string_type.h>
#include <fuzzer/types/types.h>
#include <fuzzer/utils/log.h>
#include <fuzzer/utils/random.h>
#include <ifaddrs.h>
#include <linux/wireless.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <fuzzer/types/int_type.h>

#include <fuzzer/interface/IResourceManagerClient.h>

vector<string> packageList;
vector<string> permissionList;
vector<string> mediaUrlList;

/* copyied from
 * frameworks/av/drm/mediadrm/plugins/clearkey/common/include/MimeType.h */

// const android::String8 kCencInitDataFormat("cenc");
// const android::String8 kIsoBmffAudioMimeType("audio/mp4");
// const android::String8 kIsoBmffVideoMimeType("video/mp4");
// const android::String8 kWebmInitDataFormat("webm");
// const android::String8 kWebmAudioMimeType("audio/webm");
// const android::String8 kWebmVideoMimeType("video/webm");
vector<string> mimeTypeVector = {"cenc", "audio/mp4",  "video/mp4",
                                 "webm", "audio/webm", "video/webm"};

char randomAlphanumChar() {
  char c;
  while (!std::isalnum(c = static_cast<char>(std::rand())))
    ;
  return c;
}
char randomPrintableChar() {
  char c;
  while (!std::isprint(c = static_cast<char>(std::rand())))
    ;
  return c;
}
char randomChar() { return static_cast<char>(std::rand()); }

std::string randomAlphanumString(std::string::size_type sz) {
  std::string s;
  if (sz > 100000) {
    // must smaller than 100000
    sz = 100000;
  }
  s.reserve(sz);
  generate_n(std::back_inserter(s), sz, randomAlphanumChar);
  return s;
}
std::string randomPrintableString(std::string::size_type sz) {
  std::string s;
  if (sz > 100000) {
    // must smaller than 100000
    sz = 100000;
  }
  s.reserve(sz);
  generate_n(std::back_inserter(s), sz, randomPrintableChar);
  return s;
}
std::string randomString(std::string::size_type sz) {
  std::string s;
  if (sz > 100000) {
    // must smaller than 100000
    sz = 100000;
  }
  s.reserve(sz);
  generate_n(std::back_inserter(s), sz, randomChar);
  return s;
}

string randomGenerate() {
  uint64_t flag = randomUInt64(0, 9);

  // generate random string?
  if (flag < 5) {
    // 50%
    uint64_t len = IntType<uint64_t>::generateSize();
    return randomAlphanumString(len);
  } else if (flag < 8) {
    // 30%
    uint64_t len = IntType<uint64_t>::generateSize();
    return randomPrintableString(len);
  } else if (flag < 9) {
    // 10%
    uint64_t len = IntType<uint64_t>::generateSize();
    return randomString(len);
  } else {
    // 10%
    return "";
  }
}

string StringType::generate() {
  IntType<int32_t> intType("", "");
  if (intType.nOutOf(7, 10)) {
    // consider specific semantic meaning
    FUZZER_LOGD("VarName: %s", varName.c_str());
    if (isPackage(varName) == true) { // is package name?
      value = generatePackageName();
    } else if (isPermission(varName)) {
      value = generatePermissionName();
    } else if (isNetworkInterfaceName(varName)) {
      value = generateNetworkInterfaceName();
    } else if (varName.find("KeyValuePairs") != string::npos) {
      value = generataKeyValuePairs();
    } else if (varName == "in_cameraId") {
      int32_t cameraNum = Camera::getNumberOfCameras();
      int32_t tmp = randomUInt64(0, cameraNum - 1);
      value = to_string(tmp);
    } else if (varName == "mime") {
      FUZZER_LOGD("generate mime.");
      int32_t idx = randomUInt64(0, mimeVector.size() - 1);
      value = mimeVector[idx].c_str();
      FUZZER_LOGD("mime: %s", mimeVector[idx].c_str());
    } else if (varName == "mimeType") {
      FUZZER_LOGD("generate mimeType.");
      int32_t idx = randomUInt64(0, mimeTypeVector.size() - 1);
      value = mimeTypeVector[idx].c_str();
      FUZZER_LOGD("mimeType: %s", mimeTypeVector[idx].c_str());
    } else if (varName == "algorithm") {
      // TODO: other algorithms?
      value = "HmacSHA256";
    } else if (varName.find("chainName") != string::npos) {
      value = generateChainName();
    } else if (varName.find("in_servers") != string::npos) {
      const std::vector<std::string> LOCALLY_ASSIGNED_DNS{
          "8.8.8.8", "2001:4860:4860::8888"};
      int n = randomUInt64(0, 2);
      if (n < 2) {
        value = LOCALLY_ASSIGNED_DNS[n];
      } else {
        value = generateIP();
      }
    } else if (varName.find("in_tlsServers") != string::npos) {
      value = generateIP();
    } else if (varName.find("tlsName") != string::npos) {
      value = "";
    } else if (varName.find("addrString") != string::npos) {
      value = generateIP();
    } else if (varName.find("sourceAddress") != string::npos ||
               varName.find("destinationAddress") != string::npos ||
               varName.find("localAddress") != string::npos ||
               varName.find("remoteAddress") != string::npos ||
               varName.find("ipAddress") != string::npos ||
               varName.find("ipAddr") != string::npos ||
               varName.find("srcIp") != string::npos ||
               varName.find("dstIp") != string::npos) {
      value = generateIP();
    } else if (varName == "op") {
      value = generateOp();
    } else if (isUrl(varName)) {
      value = generateUrl(varName);
    } else if (isPath(varName)) {
      value = generatePath(varName);
    } else {
      value = randomGenerate();
    }
  } else {
    value = randomGenerate();
  }
  return value;
}

string StringType::generateUrl(string varName) {
  transform(varName.begin(), varName.end(), varName.begin(), ::tolower);
  // TODO: improve the url generation...
  if (varName == "url" || varName == "srcurl") {
    // is media url, messy..
    uint32_t idx = randomUInt64(0, mediaUrlList.size() - 1);
    return mediaUrlList[idx];
  } else if (varName == "in_url") {
    // IUpdateEngine url
    return "http://127.0.0.1:8080/payload";
  } else {
    string url = "";
    if (IntType<int>::nOutOf(1, 2)) {
      url += "http://";
    } else {
      url += "https://";
    }

    int n = randomUInt64(2, 5);
    for (int i = 0; i < n; ++i) {
      int size = randomUInt64(1, 10);
      url += randomAlphanumString(size);
      if (i < n - 1) {
        url += ".";
      }
    }

    n = randomUInt64(0, 5);
    for (int i = 0; i < n; ++i) {
      int size = randomUInt64(1, 10);
      url += randomAlphanumString(size);
      if (i < n - 1) {
        url += "/";
      }
    }
    return url;
  }
}
bool StringType::isUrl(string varName) {
  transform(varName.begin(), varName.end(), varName.begin(), ::tolower);
  if (varName.find("url") != string::npos) {
    return true;
  } else {
    return false;
  }
}
string StringType::generatePath(string varName) {
  transform(varName.begin(), varName.end(), varName.begin(), ::tolower);
  if (varName.find("apk") != string::npos) {
    map<string, int>::iterator it = apkFDPool.begin();
    uint64_t r = randomUInt64(0, (uint64_t)apkFDPool.size() - 1);
    std::advance(it, r);
    return it->first;
  } else {
    int n = randomUInt64(0, 10);
    string path = "";
    for (int i = 0; i < n; ++i) {
      path = path + "/" + randomGenerate();
    }
    if (IntType<int>::nOutOf(1, 3)) {
      path = path + "/";
    }
    return path;
  }
}
bool StringType::isPath(string varName) {
  transform(varName.begin(), varName.end(), varName.begin(), ::tolower);
  return varName.find("path") != string::npos;
}

string StringType::generateOp() {
  /** Access to coarse location information. */
  static string OPSTR_COARSE_LOCATION = "android:coarse_location";
  /** Access to fine location information. */
  static string OPSTR_FINE_LOCATION = "android:fine_location";
  /** Continually monitoring location data. */
  static string OPSTR_MONITOR_LOCATION = "android:monitor_location";
  /** Continually monitoring location data with a relatively high power request.
   */
  static string OPSTR_MONITOR_HIGH_POWER_LOCATION =
      "android:monitor_location_high_power";
  /** Access to {@link android.app.usage.UsageStatsManager}. */
  static string OPSTR_GET_USAGE_STATS = "android:get_usage_stats";
  /** Activate a VPN connection without user intervention. @hide */
  static string OPSTR_ACTIVATE_VPN = "android:activate_vpn";
  /** Allows an application to read the user's contacts data. */
  static string OPSTR_READ_CONTACTS = "android:read_contacts";
  /** Allows an application to write to the user's contacts data. */
  static string OPSTR_WRITE_CONTACTS = "android:write_contacts";
  /** Allows an application to read the user's call log. */
  static string OPSTR_READ_CALL_LOG = "android:read_call_log";
  /** Allows an application to write to the user's call log. */
  static string OPSTR_WRITE_CALL_LOG = "android:write_call_log";
  /** Allows an application to read the user's calendar data. */
  static string OPSTR_READ_CALENDAR = "android:read_calendar";
  /** Allows an application to write to the user's calendar data. */
  static string OPSTR_WRITE_CALENDAR = "android:write_calendar";
  /** Allows an application to initiate a phone call. */
  static string OPSTR_CALL_PHONE = "android:call_phone";
  /** Allows an application to read SMS messages. */
  static string OPSTR_READ_SMS = "android:read_sms";
  /** Allows an application to receive SMS messages. */
  static string OPSTR_RECEIVE_SMS = "android:receive_sms";
  /** Allows an application to receive MMS messages. */
  static string OPSTR_RECEIVE_MMS = "android:receive_mms";
  /** Allows an application to receive WAP push messages. */
  static string OPSTR_RECEIVE_WAP_PUSH = "android:receive_wap_push";
  /** Allows an application to send SMS messages. */
  static string OPSTR_SEND_SMS = "android:send_sms";
  /** Required to be able to access the camera device. */
  static string OPSTR_CAMERA = "android:camera";
  /** Required to be able to access the microphone device. */
  static string OPSTR_RECORD_AUDIO = "android:record_audio";
  /** Required to access phone state related information. */
  static string OPSTR_READ_PHONE_STATE = "android:read_phone_state";
  /** Required to access phone state related information. */
  static string OPSTR_ADD_VOICEMAIL = "android:add_voicemail";
  /** Access APIs for SIP calling over VOIP or WiFi */
  static string OPSTR_USE_SIP = "android:use_sip";
  /** Access APIs for diverting outgoing calls */
  static string OPSTR_PROCESS_OUTGOING_CALLS = "android:process_outgoing_calls";
  /** Use the fingerprint API. */
  static string OPSTR_USE_FINGERPRINT = "android:use_fingerprint";
  /** Access to body sensors such as heart rate, etc. */
  static string OPSTR_BODY_SENSORS = "android:body_sensors";
  /** Read previously received cell broadcast messages. */
  static string OPSTR_READ_CELL_BROADCASTS = "android:read_cell_broadcasts";
  /** Inject mock location into the system. */
  static string OPSTR_MOCK_LOCATION = "android:mock_location";
  /** Read external storage. */
  static string OPSTR_READ_EXTERNAL_STORAGE = "android:read_external_storage";
  /** Write external storage. */
  static string OPSTR_WRITE_EXTERNAL_STORAGE = "android:write_external_storage";
  /** Required to draw on top of other apps. */
  static string OPSTR_SYSTEM_ALERT_WINDOW = "android:system_alert_window";
  /** Required to write/modify/update system settingss. */
  static string OPSTR_WRITE_SETTINGS = "android:write_settings";
  /** @hide Get device accounts. */
  static string OPSTR_GET_ACCOUNTS = "android:get_accounts";
  static string OPSTR_READ_PHONE_NUMBERS = "android:read_phone_numbers";
  /** Access to picture-in-picture. */
  static string OPSTR_PICTURE_IN_PICTURE = "android:picture_in_picture";
  /** @hide */
  static string OPSTR_INSTANT_APP_START_FOREGROUND =
      "android:instant_app_start_foreground";
  /** Answer incoming phone calls */
  static string OPSTR_ANSWER_PHONE_CALLS = "android:answer_phone_calls";
  static string OPSTR_ACCEPT_HANDOVER = "android:accept_handover";
  static string OPSTR_GPS = "android:gps";
  static string OPSTR_VIBRATE = "android:vibrate";
  static string OPSTR_WIFI_SCAN = "android:wifi_scan";
  static string OPSTR_POST_NOTIFICATION = "android:post_notification";
  static string OPSTR_NEIGHBORING_CELLS = "android:neighboring_cells";
  static string OPSTR_WRITE_SMS = "android:write_sms";
  static string OPSTR_RECEIVE_EMERGENCY_BROADCAST =
      "android:receive_emergency_broadcast";
  static string OPSTR_READ_ICC_SMS = "android:read_icc_sms";
  static string OPSTR_WRITE_ICC_SMS = "android:write_icc_sms";
  static string OPSTR_ACCESS_NOTIFICATIONS = "android:access_notifications";
  static string OPSTR_PLAY_AUDIO = "android:play_audio";
  static string OPSTR_READ_CLIPBOARD = "android:read_clipboard";
  static string OPSTR_WRITE_CLIPBOARD = "android:write_clipboard";
  static string OPSTR_TAKE_MEDIA_BUTTONS = "android:take_media_buttons";
  static string OPSTR_TAKE_AUDIO_FOCUS = "android:take_audio_focus";
  static string OPSTR_AUDIO_MASTER_VOLUME = "android:audio_master_volume";
  static string OPSTR_AUDIO_VOICE_VOLUME = "android:audio_voice_volume";
  static string OPSTR_AUDIO_RING_VOLUME = "android:audio_ring_volume";
  static string OPSTR_AUDIO_MEDIA_VOLUME = "android:audio_media_volume";
  static string OPSTR_AUDIO_ALARM_VOLUME = "android:audio_alarm_volume";
  static string OPSTR_AUDIO_NOTIFICATION_VOLUME =
      "android:audio_notification_volume";
  static string OPSTR_AUDIO_BLUETOOTH_VOLUME = "android:audio_bluetooth_volume";
  static string OPSTR_WAKE_LOCK = "android:wake_lock";
  static string OPSTR_MUTE_MICROPHONE = "android:mute_microphone";
  static string OPSTR_TOAST_WINDOW = "android:toast_window";
  static string OPSTR_PROJECT_MEDIA = "android:project_media";
  static string OPSTR_WRITE_WALLPAPER = "android:write_wallpaper";
  static string OPSTR_ASSIST_STRUCTURE = "android:assist_structure";
  static string OPSTR_ASSIST_SCREENSHOT = "android:assist_screenshot";
  static string OPSTR_TURN_SCREEN_ON = "android:turn_screen_on";
  static string OPSTR_RUN_IN_BACKGROUND = "android:run_in_background";
  static string OPSTR_AUDIO_ACCESSIBILITY_VOLUME =
      "android:audio_accessibility_volume";
  static string OPSTR_REQUEST_INSTALL_PACKAGES =
      "android:request_install_packages";
  static string OPSTR_RUN_ANY_IN_BACKGROUND = "android:run_any_in_background";
  static string OPSTR_CHANGE_WIFI_STATE = "android:change_wifi_state";
  static string OPSTR_REQUEST_DELETE_PACKAGES =
      "android:request_delete_packages";
  static string OPSTR_BIND_ACCESSIBILITY_SERVICE =
      "android:bind_accessibility_service";
  static string OPSTR_MANAGE_IPSEC_TUNNELS = "android:manage_ipsec_tunnels";
  static string OPSTR_START_FOREGROUND = "android:start_foreground";
  static string OPSTR_BLUETOOTH_SCAN = "android:bluetooth_scan";
  static vector<string> sOpToString{
      OPSTR_COARSE_LOCATION,
      OPSTR_FINE_LOCATION,
      OPSTR_GPS,
      OPSTR_VIBRATE,
      OPSTR_READ_CONTACTS,
      OPSTR_WRITE_CONTACTS,
      OPSTR_READ_CALL_LOG,
      OPSTR_WRITE_CALL_LOG,
      OPSTR_READ_CALENDAR,
      OPSTR_WRITE_CALENDAR,
      OPSTR_WIFI_SCAN,
      OPSTR_POST_NOTIFICATION,
      OPSTR_NEIGHBORING_CELLS,
      OPSTR_CALL_PHONE,
      OPSTR_READ_SMS,
      OPSTR_WRITE_SMS,
      OPSTR_RECEIVE_SMS,
      OPSTR_RECEIVE_EMERGENCY_BROADCAST,
      OPSTR_RECEIVE_MMS,
      OPSTR_RECEIVE_WAP_PUSH,
      OPSTR_SEND_SMS,
      OPSTR_READ_ICC_SMS,
      OPSTR_WRITE_ICC_SMS,
      OPSTR_WRITE_SETTINGS,
      OPSTR_SYSTEM_ALERT_WINDOW,
      OPSTR_ACCESS_NOTIFICATIONS,
      OPSTR_CAMERA,
      OPSTR_RECORD_AUDIO,
      OPSTR_PLAY_AUDIO,
      OPSTR_READ_CLIPBOARD,
      OPSTR_WRITE_CLIPBOARD,
      OPSTR_TAKE_MEDIA_BUTTONS,
      OPSTR_TAKE_AUDIO_FOCUS,
      OPSTR_AUDIO_MASTER_VOLUME,
      OPSTR_AUDIO_VOICE_VOLUME,
      OPSTR_AUDIO_RING_VOLUME,
      OPSTR_AUDIO_MEDIA_VOLUME,
      OPSTR_AUDIO_ALARM_VOLUME,
      OPSTR_AUDIO_NOTIFICATION_VOLUME,
      OPSTR_AUDIO_BLUETOOTH_VOLUME,
      OPSTR_WAKE_LOCK,
      OPSTR_MONITOR_LOCATION,
      OPSTR_MONITOR_HIGH_POWER_LOCATION,
      OPSTR_GET_USAGE_STATS,
      OPSTR_MUTE_MICROPHONE,
      OPSTR_TOAST_WINDOW,
      OPSTR_PROJECT_MEDIA,
      OPSTR_ACTIVATE_VPN,
      OPSTR_WRITE_WALLPAPER,
      OPSTR_ASSIST_STRUCTURE,
      OPSTR_ASSIST_SCREENSHOT,
      OPSTR_READ_PHONE_STATE,
      OPSTR_ADD_VOICEMAIL,
      OPSTR_USE_SIP,
      OPSTR_PROCESS_OUTGOING_CALLS,
      OPSTR_USE_FINGERPRINT,
      OPSTR_BODY_SENSORS,
      OPSTR_READ_CELL_BROADCASTS,
      OPSTR_MOCK_LOCATION,
      OPSTR_READ_EXTERNAL_STORAGE,
      OPSTR_WRITE_EXTERNAL_STORAGE,
      OPSTR_TURN_SCREEN_ON,
      OPSTR_GET_ACCOUNTS,
      OPSTR_RUN_IN_BACKGROUND,
      OPSTR_AUDIO_ACCESSIBILITY_VOLUME,
      OPSTR_READ_PHONE_NUMBERS,
      OPSTR_REQUEST_INSTALL_PACKAGES,
      OPSTR_PICTURE_IN_PICTURE,
      OPSTR_INSTANT_APP_START_FOREGROUND,
      OPSTR_ANSWER_PHONE_CALLS,
      OPSTR_RUN_ANY_IN_BACKGROUND,
      OPSTR_CHANGE_WIFI_STATE,
      OPSTR_REQUEST_DELETE_PACKAGES,
      OPSTR_BIND_ACCESSIBILITY_SERVICE,
      OPSTR_ACCEPT_HANDOVER,
      OPSTR_MANAGE_IPSEC_TUNNELS,
      OPSTR_START_FOREGROUND,
      OPSTR_BLUETOOTH_SCAN,
  };
  int32_t idx = randomUInt64(0, sOpToString.size() - 1);
  return sOpToString[idx];
}
string StringType::generateIP() {
  // here we only conside IPv4
  string ip = "";
  ip = ip + to_string(randomUInt64(0, 255)) + ".";
  ip = ip + to_string(randomUInt64(0, 255)) + ".";
  ip = ip + to_string(randomUInt64(0, 255)) + ".";
  ip = ip + to_string(randomUInt64(0, 255));
  return ip;
}

bool StringType::isChainName(string varName) {
  transform(varName.begin(), varName.end(), varName.begin(), ::tolower);
  if (varName.find("chainname") != string::npos) {
    return true;
  } else {
    return false;
  }
}

string StringType::generateChainName() {
  static vector<string> chainName{"INPUT", "OUTPUT", "FORWARD"};
  int idx = randomUInt64(0, 2);
  return chainName[idx];
}

int check_wireless(const char *ifname, char *protocol) {
  int sock = -1;
  struct iwreq pwrq;
  memset(&pwrq, 0, sizeof(pwrq));
  strncpy(pwrq.ifr_name, ifname, IFNAMSIZ);

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    FUZZER_LOGE("socket failed.");
    return 0;
  }

  if (ioctl(sock, SIOCGIWNAME, &pwrq) != -1) {
    if (protocol)
      strncpy(protocol, pwrq.u.name, IFNAMSIZ);
    close(sock);
    return 1;
  }

  close(sock);
  return 0;
}

string StringType::generateNetworkInterfaceName() {
  struct ifaddrs *ifaddr, *ifa;

  if (getifaddrs(&ifaddr) == -1) {
    FUZZER_LOGE("Failed to get ifaddrs.");
    exit(0);
  }
  map<string, vector<string>> networkInterface;
  /* Walk through linked list, maintaining head pointer so we
     can free list later */
  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    // char protocol[IFNAMSIZ] = {0};

    if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_PACKET)
      continue;
    // printf("interface %s\n", ifa->ifa_name);
    string tmp(ifa->ifa_name);
    if (tmp.find("wlan") != string::npos || tmp == "p2p0") {
      networkInterface["wireless"].push_back(ifa->ifa_name);
    } else {
      networkInterface["other"].push_back(ifa->ifa_name);
    }
    // if (check_wireless(ifa->ifa_name, protocol)) {
    //   networkInterface["wireless"].push_back(ifa->ifa_name);
    //   printf("interface %s is wireless: %s\n", ifa->ifa_name, protocol);
    // } else {
    //   networkInterface["other"].push_back(ifa->ifa_name);
    // }
  }
  freeifaddrs(ifaddr);
  if (networkInterface["wireless"].size() > 0) {
    uint32_t idx = randomUInt64(0, networkInterface["wireless"].size() - 1);
    return networkInterface["wireless"][idx];
  } else if (networkInterface["other"].size() > 0) {
    uint32_t idx = randomUInt64(0, networkInterface["other"].size() - 1);
    return networkInterface["other"][idx];
  } else {
    return "";
  }
}

bool StringType::isNetworkInterfaceName(string varName) {
  transform(varName.begin(), varName.end(), varName.begin(), ::tolower);
  if (varName.find("iface") != string::npos ||
      varName.find("ifname") != string::npos) {
    return true;
  } else {
    return false;
  }
}

bool StringType::isPackage(string varName) {
  transform(varName.begin(), varName.end(), varName.begin(), ::tolower);

  if (varName.find("package") != string::npos &&
      varName.find("name") != string::npos) {
    return true;
  } else if (varName.find("pkgname") != string::npos) {
    return true;
  } else if (varName.find("in_app") != string::npos) {
    return true;
  } else {
    return false;
  }
}
string StringType::generatePackageName() {
  uint32_t idx = (uint32_t)randomUInt64(0, packageList.size() - 1);
  return packageList[idx];
}

bool StringType::isPermission(string varName) {
  transform(varName.begin(), varName.end(), varName.begin(), ::tolower);
  if (varName.find("permission") != string::npos) {
    return true;
  } else {
    return false;
  }
}
string StringType::generatePermissionName() {
  uint32_t idx = (uint32_t)randomUInt64(0, permissionList.size() - 1);
  return permissionList[idx];
}

void initPackageNameList() {
  ifstream in;
  in.open(PACKAGE_LIST_PATH);
  string packageName;
  while (getline(in, packageName)) {
    packageList.push_back(packageName);
  }
  FUZZER_LOGI("Package name list has been already inited.\n");
}

void initMediaUrlList() {
  ifstream in;
  in.open(MEDIA_URL_LIST_PATH);
  string mediaUrl;
  while (getline(in, mediaUrl)) {
    mediaUrlList.push_back(mediaUrl);
  }
  FUZZER_LOGI("Media url list has been already inited.\n");
}
void initPermissionNameList() {
  ifstream in;
  in.open(PERMISSION_LIST_PATH);
  string permissionName;
  while (getline(in, permissionName)) {
    permissionList.push_back(permissionName);
  }
  FUZZER_LOGI("Permission name list has been already inited.\n");
}

string StringType::generateAudioParameterKeyPair() {
  map<string, string> audioParameter;
  audioParameter["bt_headset_nrec"] = "on|off";
  audioParameter["hw_av_sync"] = "string";
  audioParameter["screen_state"] = "on|off";
  audioParameter["routing"] = "audio_devices_t";
  audioParameter["format"] = "enum audio_format_t";
  audioParameter["channels"] = "audio_channel_mask_t";
  audioParameter["frame_count"] = "size_t";
  audioParameter["input_source"] = "enum audio_source_t";
  audioParameter["sampling_rate"] = "uint32_t";
  audioParameter["presentation_id"] = "int32_t";
  audioParameter["program_id"] = "int32_t";
  audioParameter["connect"] = "audio_devices_t";
  audioParameter["disconnect"] = "audio_devices_t";
  audioParameter["mono_output"] = "bool";
  audioParameter["sup_formats"] = "enum audio_format_t";
  audioParameter["sup_channels"] = "audio_channel_mask_t";
  audioParameter["sup_sampling_rates"] = "int32_t";
  audioParameter["reconfigA2dp"] = "int32_t";            // do not know
  audioParameter["isReconfigA2dpSupported"] = "int32_t"; // do not know
  uint32_t num = randomUInt64(0, audioParameter.size() - 1);
  string result = "";
  while (num--) {
    uint64_t r = randomUInt64(0, (uint64_t)audioParameter.size() - 1);
    auto it = audioParameter.begin();
    std::advance(it, r);
    result = result + it->first + "=";
    if (it->second == "on|off") {
      r = randomUInt64(0, 1);
      if (r == 0) {
        result = result + "on";
      } else {
        result = result + "off";
      }
    } else {
      VarType varTypeEnum = getVarTypeEnum(it->second);
      switch (varTypeEnum) {
      case INT32_TYPE: {
        IntType<int32_t> intType("", it->second);
        int32_t value = intType.generate();
        result = result + to_string(value);
        break;
      }
      case UINT32_TYPE: {
        IntType<uint32_t> intType("", it->second);
        uint32_t value = intType.generate();
        result = result + to_string(value);
        break;
      }
      case INT64_TYPE: {
        IntType<int64_t> intType("", it->second);
        int64_t value = intType.generate();
        result = result + to_string(value);
        break;
      }
      case UINT64_TYPE: {
        IntType<uint64_t> intType("", it->second);
        uint64_t value = intType.generate();
        result = result + to_string(value);
        break;
      }
      case CSTRING_TYPE: {
        StringType stringType("", it->second);
        string value = stringType.generate();
        result = result + value;
        break;
      }
      case BOOL_TYPE: {
        BoolType boolType("", it->second);
        bool value = boolType.generate();
        result = result + to_string(value);
        break;
      }
      default: {
        break;
      }
      }
    }
    result = result + ";";
  }
  return result;
}

string StringType::generataKeyValuePairs() {
  int n = randomUInt64(0, 5);
  if (n == 0) {
    return generateAudioParameterKeyPair();
  } else {
    string prefix = randomGenerate();
    string suffix = randomGenerate();
    return prefix + "=" + suffix;
  }
}

vector<string> split(const string &str, const string &pattern) {
  vector<string> res;
  if (str == "")
    return res;
  string strs = str + pattern;
  size_t pos = strs.find(pattern);

  while (pos != strs.npos) {
    string temp = strs.substr(0, pos);
    res.push_back(temp);
    strs = strs.substr(pos + pattern.size(), strs.size());
    pos = strs.find(pattern);
  }
  return res;
}