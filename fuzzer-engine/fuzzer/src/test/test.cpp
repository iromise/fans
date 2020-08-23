#include <fuzzer/interface/interface.h>

#include <fuzzer/executor.h>
#include <fuzzer/generator.h>

#include <fuzzer/service.h>
#include <fuzzer/test.h>
#include <fuzzer/types/types.h>

#include <fuzzer/utils/log.h>
#include <fuzzer/utils/util.h>
#include <iostream>
#include <stdio.h>
using namespace std;

void testRun(Transaction &tx) {
  Executor executor;
  status_t ret;
  ParcelReaderWriter parcelReaderWriter(tx.info["data"], tx.variable, tx.loop,
                                        tx.constraint);
  parcelReaderWriter.initTxWrite(&tx);
  parcelReaderWriter.start();
  tx.flags = 0; // sync
  ret = executor.run(tx);
  return;
}

void testIsCryptoSchemeSupported() {
  string txName = "ICrypto::2-2";
  Json::Value &txMeta = svcInfo[txName];
  uint32_t possIdx = 1;
  Transaction tx(txName, txMeta, possIdx);
  testRun(tx);
  cout << "dataSize: " << tx.reply.dataSize() << endl;
  cout << "dataAvail: " << tx.reply.dataAvail() << endl;
  int32_t supported = tx.reply.readInt32();
  cout << "isCryptoSchemeSupported:" << supported << endl;
  return;
}
/*IDrm*/
void testIsCryptoSchemeSupportedForMimeType() {
  string txName = "IDrm::2-2";
  Json::Value &txMeta = svcInfo[txName];
  uint32_t possIdx = 1;
  Transaction tx(txName, txMeta, possIdx);
  testRun(tx);
  cout << "dataSize: " << tx.reply.dataSize() << endl;
  cout << "dataAvail: " << tx.reply.dataAvail() << endl;
  int32_t supported = tx.reply.readInt32();
  cout << "isCryptoSchemeSupported:" << supported << endl;
  return;
}

void testOpenSeesion() {
  string txName = "IDrm::3-3";
  uint32_t possIdx = 1;
  Transaction tx_pre(txName, svcInfo[txName], possIdx);
  testRun(tx_pre);
  int32_t createStatus = tx_pre.reply.readInt32();
  cout << "status of createPlugin:" << createStatus << endl;

  txName = "IDrm::5-5";
  possIdx = 1;
  Transaction tx(txName, svcInfo[txName], possIdx);
  testRun(tx);
  cout << "dataSize: " << tx.reply.dataSize() << endl;
  cout << "dataAvail: " << tx.reply.dataAvail() << endl;
  vector<int8_t> buffer;
  tx.reply.readByteVector(&buffer);
  FUZZER_LOGD("Array size: %lu", buffer.size());
  string result = "";
  for (auto i : buffer) {
    result += to_string(i) + " ";
  }
  FUZZER_LOGD("Array content: %s", result.c_str());
  int32_t status = tx.reply.readInt32();
  cout << "status of openSession:" << status << endl;
  return;
}
void testCloseSession() {
  string txName = "IDrm::3-3";
  uint32_t possIdx = 1;
  Transaction tx_pre(txName, svcInfo[txName], possIdx);
  testRun(tx_pre);

  txName = "IDrm::6-6";
  possIdx = 1;
  Transaction tx(txName, svcInfo[txName], possIdx);
  testRun(tx);
  cout << "dataSize: " << tx.reply.dataSize() << endl;
  cout << "dataAvail: " << tx.reply.dataAvail() << endl;
  int32_t status = tx.reply.readInt32();
  cout << "status of closeSession:" << status << endl;
  return;
}