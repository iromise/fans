#ifndef GENERATOR_H
#define GENERATOR_H
#include <fuzzer/service.h>
#include <fuzzer/transaction.h>
#include <fuzzer/utils/log.h>
#include <fuzzer/utils/random.h>
#include <json/json.h>
class Generator {
public:
  Transaction generateTx();
};
#endif // GENERATOR_H