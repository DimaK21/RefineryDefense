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

/* fx_within must agree exactly with fx_dist(dx, dy) <= r (floor sqrt), boundaries included. */
static void test_fx_within(void) {
  static const int32_t rs[] = {0, 1, 12 * FX, 13 * FX, 22 * FX, 70 * FX, 80 * FX};
  for (unsigned k = 0; k < sizeof(rs) / sizeof(rs[0]); k++) {
    int32_t r = rs[k];
    for (int32_t dx = -r - 3; dx <= r + 3; dx += (r > 64 ? r / 16 : 1))
      for (int32_t dy = -2; dy <= 2; dy++) {
        CHECK_EQ(fx_within(dx, dy, r), fx_dist(dx, dy) <= r);
        CHECK_EQ(fx_within(dy, dx, r), fx_dist(dy, dx) <= r);
      }
    CHECK(fx_within(r, 1, r) == (fx_dist(r, 1) <= r));
    CHECK(fx_within(r + 1, 0, r) == 0);
  }
  CHECK(fx_within(13 * FX, 1, 13 * FX));         /* floor(sqrt) == r: inside, unlike dx*dx+dy*dy <= r*r */
  CHECK(!fx_within(13 * FX + 1, 0, 13 * FX));
  CHECK(!fx_within(300 * FX, 300 * FX, 80 * FX)); /* far off-world: no 32-bit overflow */
  CHECK(fx_within(-5 * FX, 0, 5 * FX));
  CHECK(!fx_within(0, 0, -1));
}

static void test_rng(void) {
  uint32_t a = 42, b = 42;
  for (int i = 0; i < 100; i++) CHECK_EQ(rng_next(&a), rng_next(&b));
  uint32_t s = 7;
  for (int i = 0; i < 1000; i++) { int32_t v = rng_range(&s, -3, 3); CHECK(v >= -3 && v <= 3); }
}

int main(void) {
  RUN(test_fx);
  RUN(test_fx_within);
  RUN(test_rng);
  return TEST_SUMMARY();
}
