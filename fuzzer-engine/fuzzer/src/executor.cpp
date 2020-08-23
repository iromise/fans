#include <binder/Binder.h>
#include <binder/Parcel.h>
#include <fuzzer/executor.h>
#include <fuzzer/service.h>
#include <fuzzer/utils/log.h>
#include <stdio.h>
status_t Executor::run(Transaction &tx) {
  NativeServiceManager nsm;
  sp<IBinder> interface = nsm.getService(tx);
  if (interface == NULL) {
    // clear tx status
    usedTxs.erase(tx.txName);
    // TODO: consider when this tx is used by some dependency.
    return 0;
  }
  FUZZER_LOGI("Start issuing transaction %s.", tx.txName.c_str());
  status_t ret = interface->transact(tx.code, tx.data, &tx.reply, tx.flags);
  FUZZER_LOGI("Transaction return status: %d.", ret);
  // clear tx status
  usedTxs.erase(tx.txName);
  return ret;
}