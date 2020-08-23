#include <fuzzer/constraint_checker.h>

#include <fuzzer/types/bool_type.h>
#include <fuzzer/utils/log.h>
#include <fuzzer/utils/random.h>
using namespace std;

bool BoolType::generate() {

  // consider specific semantic meaning

  // random bool;
  value = (bool)randomUInt64(0, 1);

  return value;
}