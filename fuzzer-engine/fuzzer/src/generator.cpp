#include <fuzzer/generator.h>
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/utils/log.h>
#include <fuzzer/utils/random.h>
Transaction Generator::generateTx() {
  map<string, Json::Value>::iterator it;
  if (targetTransactionInfo.size() == 0) {
    uint64_t r = randomUInt64(0, (uint64_t)svcInfo.size() - 1);
    it = svcInfo.begin();
    std::advance(it, r);
  } else {
    uint64_t r = randomUInt64(0, (uint64_t)targetTransactionInfo.size() - 1);
    it = targetTransactionInfo.begin();
    std::advance(it, r);
  }

  string txName(it->first);
  Json::Value &txMeta = it->second;
  uint64_t len = txMeta["possibility"].size();
  uint32_t possIdx = (uint32_t)randomUInt64(0, len - 1);
  FUZZER_LOGI("Random choose index %u in transaction %s.", possIdx,
              it->first.c_str());
  Transaction tx(txName, txMeta, possIdx);
  ParcelReaderWriter parcelReaderWriter(tx.info["data"], tx.variable, tx.loop,
                                        tx.constraint);
  parcelReaderWriter.initTxWrite(&tx);
  parcelReaderWriter.start();
  return tx;
}
