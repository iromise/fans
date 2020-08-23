#include <fuzzer/interface/interface.h>

#include <fuzzer/constraint_checker.h>

#include <fuzzer/dependency_solver.h>
#include <fuzzer/types/binder_type.h>
#include <fuzzer/utils/log.h>
#include <fuzzer/utils/random.h>
#include <stdio.h>

using namespace std;

sp<IBinder> BinderType::generate() {
  sp<IBinder> binder;
  // Using the valid interface with a large probability
  if (IntType<int32_t>::nOutOf(7, 10)) {
    binder = generateInterface(interfaceName, varName);
    if (binder == NULL) {
      FUZZER_LOGI(
          "Failed to generate interface manually, so we use simple binder.");
      binder = new BBinder();
    } else {
      FUZZER_LOGD("Manually generate interface successfully.");
    }
  } else {
    // may be we can trigger NULL Pointer dereference?
    binder = NULL;
  }
  return binder;
}