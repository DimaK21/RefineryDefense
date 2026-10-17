#include <string.h>
#include "test.h"
#include "config.h"
#include "waves.h"

static int threat(const WaveEntry *e) { return DRONE_SPECS[e->type].threat; }

static void test_waves(void) {
  CHECK_EQ(wave_budget(1, 2), 32); CHECK_EQ(wave_budget(1, 4), 38); CHECK_EQ(wave_budget(10, 1), 140);
  WaveEntry w[MAX_WAVE], w2[MAX_WAVE];
  uint16_t n = wave_generate(w, MAX_WAVE, 1, 2, 123);
  CHECK(n > 0);
  int sum = 0;
  for (uint16_t i = 0; i < n; i++) {
    CHECK_EQ(w[i].type, DR_SWITCHBLADE); sum += threat(&w[i]); CHECK(w[i].side == w[0].side);
    CHECK(w[i].time_ms < SPAWN_WINDOW_MS);
  }
  CHECK(sum <= 32);
  for (uint16_t i = 1; i < n; i++) CHECK(w[i - 1].time_ms <= w[i].time_ms);
  CHECK_EQ(wave_generate(w2, MAX_WAVE, 1, 2, 123), n); CHECK(memcmp(w, w2, n * sizeof *w) == 0);
  n = wave_generate(w, MAX_WAVE, 3, 2, 9);
  for (uint16_t i = 0; i < n; i++) CHECK(w[i].type != DR_HARPY && w[i].type != DR_SHAHED238);
  n = wave_generate(w, MAX_WAVE, 8, 2, 9);
  int seen[4] = {0, 0, 0, 0};
  for (uint16_t i = 0; i < n; i++) { CHECK(w[i].side < 4); seen[w[i].side] = 1; }
  CHECK(seen[0] && seen[1] && seen[2] && seen[3]);
  n = wave_generate(w, MAX_WAVE, 200, 5, 9); CHECK(n <= MAX_WAVE);
  CHECK_EQ(wave_generate(w, 7, 200, 5, 9), 7);                 /* small pool truncates */
  CHECK_EQ(wave_generate(w, MAX_WAVE, 1, 2, 0) > 0, 1);        /* zero seed is valid */
}

int main(void) {
  RUN(test_waves);
  return TEST_SUMMARY();
}
