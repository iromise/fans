#include <fuzzer/constraint_checker.h>
#include <fuzzer/types/double_type.h>
#include <fuzzer/utils/log.h>
#include <fuzzer/utils/random.h>
using namespace std;

double DoubleType::generate() {
  // consider specific semantic meaning

  // random Double
  value = randomDouble(__DBL_MIN__, __DBL_MAX__);
  return value;
}