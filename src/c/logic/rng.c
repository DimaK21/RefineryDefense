#include "rng.h"

uint32_t rng_next(uint32_t *state) {
  uint32_t x = *state;
  if (x == 0) x = 0x9E3779B9u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

int32_t rng_range(uint32_t *state, int32_t lo, int32_t hi) {
  uint32_t span = (uint32_t)(hi - lo) + 1u;
  return lo + (int32_t)(rng_next(state) % span);
}
