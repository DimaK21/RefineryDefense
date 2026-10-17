#pragma once
#include <stdint.h>

uint32_t rng_next(uint32_t *state);
int32_t rng_range(uint32_t *state, int32_t lo, int32_t hi);
