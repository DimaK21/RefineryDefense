#include "fx.h"

/* Quarter period, 65 entries: round(256 * sin(i * 90deg / 64)). */
static const uint16_t s_quarter[65] = {
  0, 6, 13, 19, 25, 31, 38, 44, 50, 56, 62, 68, 74, 80, 86, 92, 98, 104, 109,
  115, 121, 126, 132, 137, 142, 147, 152, 157, 162, 167, 172, 177, 181, 185,
  190, 194, 198, 202, 206, 209, 213, 216, 220, 223, 226, 229, 231, 234, 237,
  239, 241, 243, 245, 247, 248, 250, 251, 252, 253, 254, 255, 255, 256, 256,
  256
};

int32_t fx_sin(uint16_t angle) {
  uint32_t quadrant = angle >> 14;       /* 0..3 */
  uint32_t idx = (angle & 0x3FFF) >> 8;  /* 0..63 */
  int32_t v;
  if (quadrant & 1) idx = 64 - idx;      /* mirror within quadrant */
  v = s_quarter[idx];
  return (quadrant & 2) ? -v : v;
}

int32_t fx_cos(uint16_t angle) {
  return fx_sin((uint16_t)(angle + 16384));
}

uint32_t isqrt32(uint32_t v) {
  uint32_t res = 0, bit = 1u << 30;
  while (bit > v) bit >>= 2;
  while (bit) {
    if (v >= res + bit) { v -= res + bit; res = (res >> 1) + bit; }
    else res >>= 1;
    bit >>= 2;
  }
  return res;
}

int32_t fx_dist(int32_t dx, int32_t dy) {
  uint64_t sq = (uint64_t)((int64_t)dx * dx) + (uint64_t)((int64_t)dy * dy);
  /* isqrt of a 64-bit value: binary digit-by-digit. */
  uint64_t res = 0, bit = (uint64_t)1 << 62;
  while (bit > sq) bit >>= 2;
  while (bit) {
    if (sq >= res + bit) { sq -= res + bit; res = (res >> 1) + bit; }
    else res >>= 1;
    bit >>= 2;
  }
  return (int32_t)res;
}
