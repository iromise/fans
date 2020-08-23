#include <fuzzer/constraint_checker.h>
#include <fuzzer/types/float_type.h>
#include <fuzzer/utils/log.h>
#include <fuzzer/utils/random.h>
using namespace std;

float FloatType::generate() {
  // consider specific semantic meaning

  // random float;
  value = randomFloat(__FLT_MIN__, __FLT_MAX__);
  return value;
}