#include "test.h"
#include "fx.h"
#include "rng.h"

static void test_fx(void) {
  CHECK_EQ(fx_sin(0), 0);
  CHECK_EQ(fx_sin(16384), 256);
  CHECK_EQ(fx_sin(49152), -256);
  CHECK_EQ(fx_cos(0), 256);
  CHECK_EQ(isqrt32(144), 12);
  CHECK_EQ(isqrt32(145), 12);
  CHECK_EQ(fx_dist(3*FX, 4*FX), 5*FX);
}

static void test_rng(void) {
  uint32_t a = 42, b = 42;
  for (int i = 0; i < 100; i++) CHECK_EQ(rng_next(&a), rng_next(&b));
  uint32_t s = 7;
  for (int i = 0; i < 1000; i++) { int32_t v = rng_range(&s, -3, 3); CHECK(v >= -3 && v <= 3); }
}

int main(void) {
  RUN(test_fx);
  RUN(test_rng);
  return TEST_SUMMARY();
}
