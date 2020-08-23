#include <algorithm>
#include <fuzzer/constraint_checker.h>
#include <fuzzer/transaction.h>
#include <fuzzer/types/types.h>
#include <fuzzer/utils/log.h>
#include <fuzzer/utils/random.h>
#include <iostream>
#include <stdio.h>
#include <string>
using namespace std;
map<string, bool> usedTxs;

Transaction::Transaction() {}
Transaction::Transaction(const Transaction &) {}
Transaction::Transaction(string &txName, Json::Value &txMeta,
                         uint32_t &possIdx) {
  FUZZER_LOGI("Start initing the Transaction.");
  this->txName = txName;
  // record used txs
  usedTxs[txName] = true;
  serviceName = txMeta["serviceName"].asString();
  interfaceName = txMeta["interfaceName"].asString();
  interfaceToken = txMeta["interfaceToken"].asString();
  uint32_t codeIdx = randomUInt64(0, txMeta["code"].size() - 1);
  code = txMeta["code"][codeIdx].asUInt();
  info = txMeta["possibility"][possIdx];
  dependency = txMeta["dependency"];
  variable = txMeta["variable"];
  loop = txMeta["loop"];
  constraint = txMeta["constraint"];
  flags = 1; // default async
  FUZZER_LOGD("Basic info about this transaction:");
  FUZZER_LOGD("   txName %s", txName.c_str());
  FUZZER_LOGD("   serviceName  %s", serviceName.c_str());
  FUZZER_LOGD("   interfaceName  %s", interfaceName.c_str());
  FUZZER_LOGD("   interfaceToken %s", interfaceToken.c_str());
  FUZZER_LOGD("   Code %lu", code);
}