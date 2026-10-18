#pragma once
#include <stddef.h>
#include "types.h"

#define SAVE_VERSION 2
/* version1 pending1 night2 money4 score4 seed4 rng4 + 5x(status1,hp1) difficulty1; little-endian.
 * Version 1 had no difficulty byte (30 bytes) and still loads, as Normal. */
#define SAVE_HDR_LEN 31
#define SAVE_HDR_LEN_V1 30
#define SAVE_TOWERS_LEN 201  /* count1 + 40x(type,col,row,hp,ammo) */
#define SAVE_LEN (SAVE_HDR_LEN + SAVE_TOWERS_LEN)   /* one persist key: header then towers */
#define SAVE_LEN_V1 (SAVE_HDR_LEN_V1 + SAVE_TOWERS_LEN)

size_t save_write_header(const Game *g, bool pending_night, uint8_t *buf, size_t cap);
size_t save_write_towers(const Game *g, uint8_t *buf, size_t cap);
bool   save_read(Game *g, bool *pending_night, const uint8_t *hdr, size_t hdr_len,
                 const uint8_t *tw, size_t tw_len);
size_t save_write(const Game *g, bool pending_night, uint8_t *buf, size_t cap);
bool   save_read_blob(Game *g, bool *pending_night, const uint8_t *buf, size_t len);   /* v2 or v1 length */

typedef struct { uint16_t nights; uint32_t score; } Best;
bool   best_update(Best *b, uint16_t nights, uint32_t score);
