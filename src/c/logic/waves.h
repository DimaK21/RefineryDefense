#pragma once
#include "types.h"

int32_t wave_budget(uint16_t night, int alive_objects);
/* Fills out[] sorted by time_ms (all < SPAWN_WINDOW_MS); returns entry count (<= max). */
uint16_t wave_generate(WaveEntry *out, uint16_t max, uint16_t night, int alive_objects, uint32_t seed);
