#pragma once
#include "types.h"

int32_t wave_budget(uint16_t night, int alive_objects);
/* Fills out[] sorted by time_ms (all < SPAWN_WINDOW_MS); returns entry count (<= max). */
/* Edges the night's drones come from (0 top, 1 right, 2 bottom, 3 left): the same ones
 * wave_generate picks for this night and seed. Fills out[0..n-1], returns n. */
int wave_sides(uint16_t night, uint32_t seed, uint8_t out[4]);
uint16_t wave_generate(WaveEntry *out, uint16_t max, uint16_t night, int alive_objects, uint32_t seed);
