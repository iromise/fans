#ifndef SERVICE_H
#define SERVICE_H
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <binder/ProcessState.h>
#include <binder/TextOutput.h>
#include <fuzzer/transaction.h>
#include <fuzzer/utils/util.h>
#include <json/json.h>
#include <string>
using namespace android;
using namespace std;
extern map<string, Json::Value> svcInfo;
extern map<string, Json::Value> targetTransactionInfo;

#define SERVICE_INFO_DIR FUZZER_PATH "model/service"

class NativeServiceManager
{
  sp<IServiceManager> sm;
  sp<IBinder> generateInterfaceManually(string serviceName);

public:
  NativeServiceManager();
  sp<IBinder> getService(Transaction &tx);
  static String16 getInterfaceName(sp<IBinder> service);
};
extern void loadServiceInfo(char *serviceInfoDir, char *targetInterface,
                            char *targetTransaction);
#endif // SERVICE_H