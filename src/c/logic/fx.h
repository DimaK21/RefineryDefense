#pragma once
#include <stdint.h>
#include <stdbool.h>

#define FX 256

int32_t fx_sin(uint16_t angle);
int32_t fx_cos(uint16_t angle);
uint32_t isqrt32(uint32_t v);
int32_t fx_dist(int32_t dx, int32_t dy);
/* Exactly fx_dist(dx, dy) <= r, without the square root. */
bool fx_within(int32_t dx, int32_t dy, int32_t r);
