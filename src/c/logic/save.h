#pragma once
#include <stddef.h>
#include "types.h"

#define SAVE_VERSION 1
#define SAVE_HDR_LEN 30      /* version1 pending1 night2 money4 score4 seed4 rng4 + 5x(status1,hp1); little-endian */
#define SAVE_TOWERS_LEN 201  /* count1 + 40x(type,col,row,hp,ammo) */
#define SAVE_LEN (SAVE_HDR_LEN + SAVE_TOWERS_LEN)   /* one persist key: header then towers */

size_t save_write_header(const Game *g, bool pending_night, uint8_t *buf, size_t cap);
size_t save_write_towers(const Game *g, uint8_t *buf, size_t cap);
bool   save_read(Game *g, bool *pending_night, const uint8_t *hdr, size_t hdr_len,
                 const uint8_t *tw, size_t tw_len);
size_t save_write(const Game *g, bool pending_night, uint8_t *buf, size_t cap);
bool   save_read_blob(Game *g, bool *pending_night, const uint8_t *buf, size_t len);

typedef struct { uint16_t nights; uint32_t score; } Best;
bool   best_update(Best *b, uint16_t nights, uint32_t score);
