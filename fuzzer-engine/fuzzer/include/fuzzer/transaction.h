#ifndef TRANSACTION_H
#define TRANSACTION_H
#include <binder/Parcel.h>
#include <json/json.h>
#include <string>
#include <vector>
using namespace std;
using namespace android;
class Transaction {
public:
  string txName;
  string serviceName;
  string interfaceName;
  string interfaceToken;
  uint64_t code;
  Json::Value info;
  Json::Value dependency;
  Json::Value variable;
  Json::Value constraint;
  Json::Value loop;
  Parcel data;
  Parcel reply;
  uint64_t flags;
  status_t ret;

  // vector<Transaction> txSeq;
  Transaction();
  Transaction(const Transaction &);
  Transaction(string &txName, Json::Value &txMeta, uint32_t &possIdx);
};
extern map<string, bool> usedTxs;
#endif // TRANSACTION_H
