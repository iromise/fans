#include <fuzzer/utils/random.h>

using namespace std;
/**
 * @brief return a random int64 between [min,max].
 *
 * @param min
 * @param max
 * @return uint64_t
 */
uint64_t randomUInt64(uint64_t min, uint64_t max) {
  random_device rd;
  mt19937_64 e(rd());
  uniform_int_distribution<uint64_t> u(min, max);
  return u(e);
}

float randomFloat(float min, float max) {
  random_device rd;
  mt19937_64 e(rd());
  uniform_real_distribution<float> u(min, max);
  return u(e);
}

double randomDouble(double min, double max) {
  random_device rd;
  mt19937_64 e(rd());
  uniform_real_distribution<double> u(min, max);
  return u(e);
}