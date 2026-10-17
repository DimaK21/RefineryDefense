#pragma once
#include <stdint.h>

#define FX 256

int32_t fx_sin(uint16_t angle);
int32_t fx_cos(uint16_t angle);
uint32_t isqrt32(uint32_t v);
int32_t fx_dist(int32_t dx, int32_t dy);
