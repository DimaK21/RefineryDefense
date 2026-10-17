#include "waves.h"
#include "config.h"
#include "rng.h"

int32_t wave_budget(uint16_t night, int alive_objects) {
  int extra = alive_objects > 2 ? alive_objects - 2 : 0;
  return (WAVE_BASE + WAVE_PER_NIGHT * (int32_t)night + (int32_t)night * night * WAVE_NIGHT_SQ) * (100 + WAVE_EXTRA_OBJ_PCT * extra) / 100;
}

static int side_count(uint16_t night) { return night <= 2 ? 1 : night <= 5 ? 2 : 4; }

static void add(WaveEntry *out, uint16_t *n, uint32_t t, uint8_t type, uint8_t side) {
  if (t >= SPAWN_WINDOW_MS) t = SPAWN_WINDOW_MS - 1;
  out[*n].time_ms = (uint16_t)t; out[*n].type = type; out[*n].side = side; (*n)++;
}

uint16_t wave_generate(WaveEntry *out, uint16_t max, uint16_t night, int alive_objects, uint32_t seed) {
  uint32_t rng = seed ? seed : 1;
  int32_t rem = wave_budget(night, alive_objects);
  uint8_t sides[4] = {0, 1, 2, 3};
  int ns = side_count(night);
  for (int i = 0; i < ns; i++) {                 /* partial Fisher-Yates: first ns are the active sides */
    int j = rng_range(&rng, i, 3); uint8_t t = sides[i]; sides[i] = sides[j]; sides[j] = t;
  }
  uint16_t n = 0;
  while (rem >= 1 && n < max) {
    uint8_t ok[DR_COUNT]; int nok = 0;
    for (int d = 0; d < DR_COUNT; d++)
      if (DRONE_SPECS[d].first_night <= night && DRONE_SPECS[d].threat <= rem) ok[nok++] = (uint8_t)d;
    if (nok == 0) break;
    uint8_t type = ok[rng_range(&rng, 0, nok - 1)];
    uint8_t side = sides[rng_range(&rng, 0, ns - 1)];
    uint32_t t = (uint32_t)rng_range(&rng, 0, SPAWN_WINDOW_MS - 1);
    int count = type == DR_SWITCHBLADE ? rng_range(&rng, SWARM_MIN, SWARM_MAX) : 1;
    for (int k = 0; k < count && n < max && DRONE_SPECS[type].threat <= rem; k++) {
      add(out, &n, t + (uint32_t)k * SWARM_INTERVAL_MS, type, side);
      rem -= DRONE_SPECS[type].threat;
    }
  }
  for (uint16_t i = 1; i < n; i++) {             /* insertion sort by time (stable) */
    WaveEntry e = out[i]; int j = i - 1;
    while (j >= 0 && out[j].time_ms > e.time_ms) { out[j + 1] = out[j]; j--; }
    out[j + 1] = e;
  }
  return n;
}
