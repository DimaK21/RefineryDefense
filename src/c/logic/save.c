#include <string.h>
#include "save.h"
#include "board.h"
#include "config.h"
#include "economy.h"

static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_u32(uint8_t *p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i));
}
static uint16_t get_u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get_u32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

size_t save_write_header(const Game *g, bool pending_night, uint8_t *buf, size_t cap) {
  if (cap < SAVE_HDR_LEN) return 0;
  buf[0] = SAVE_VERSION;
  buf[1] = pending_night ? 1 : 0;
  put_u16(buf + 2, g->night);
  put_u32(buf + 4, (uint32_t)g->money);
  put_u32(buf + 8, g->score);
  put_u32(buf + 12, g->seed);
  put_u32(buf + 16, g->rng);
  for (int i = 0; i < OBJ_COUNT; i++) {
    buf[20 + 2 * i] = g->objects[i].status;
    buf[21 + 2 * i] = g->objects[i].hp;
  }
  buf[30] = g->difficulty;
  return SAVE_HDR_LEN;
}

size_t save_write_towers(const Game *g, uint8_t *buf, size_t cap) {
  if (cap < SAVE_TOWERS_LEN) return 0;
  memset(buf, 0, SAVE_TOWERS_LEN);
  int n = 0;
  for (int i = 0; i < MAX_TOWERS; i++) {
    const Tower *t = &g->towers[i];
    if (!t->active) continue;
    uint8_t *p = buf + 1 + 5 * n++;
    p[0] = t->type; p[1] = t->col; p[2] = t->row; p[3] = t->hp; p[4] = t->ammo;
  }
  buf[0] = (uint8_t)n;
  return SAVE_TOWERS_LEN;
}

bool save_read(Game *g, bool *pending_night, const uint8_t *hdr, size_t hdr_len,
               const uint8_t *tw, size_t tw_len) {
  if (!g || !hdr || !tw || tw_len != SAVE_TOWERS_LEN) return false;
  bool v1 = hdr_len == SAVE_HDR_LEN_V1 && hdr[0] == 1;
  if (!v1 && (hdr_len != SAVE_HDR_LEN || hdr[0] != SAVE_VERSION)) return false;
  uint8_t diff = v1 ? DIFF_NORMAL : hdr[30];
  if (hdr[1] > 1 || diff >= DIFF_COUNT) return false;
  if (get_u16(hdr + 2) < 1) return false;
  for (int i = 0; i < OBJ_COUNT; i++) {
    uint8_t st = hdr[20 + 2 * i], hp = hdr[21 + 2 * i];
    if (st > OBJ_DESTROYED || hp > OBJECT_MAX_HP) return false;
    if (st == OBJ_ALIVE && hp == 0) return false;
  }
  unsigned count = tw[0];
  if (count > MAX_TOWERS) return false;
  uint8_t used[(GRID_COLS * GRID_ROWS + 7) / 8];
  memset(used, 0, sizeof(used));
  for (unsigned i = 0; i < count; i++) {
    const uint8_t *p = tw + 1 + 5 * i;
    if (p[0] >= TW_COUNT || p[1] >= GRID_COLS || p[2] >= GRID_ROWS) return false;
    const TowerSpec *s = &TOWER_SPECS[p[0]];
    if (p[3] > s->hp || p[4] > s->ammo) return false;
    if (object_at(g, p[1], p[2]) >= 0) return false;
    unsigned bit = (unsigned)p[2] * GRID_COLS + p[1];
    if (used[bit >> 3] & (1u << (bit & 7))) return false;
    used[bit >> 3] |= (uint8_t)(1u << (bit & 7));
  }
  memset(g, 0, sizeof(*g));
  g->difficulty = diff;
  g->night = get_u16(hdr + 2);
  g->money = (int32_t)get_u32(hdr + 4);
  g->score = get_u32(hdr + 8);
  g->seed = get_u32(hdr + 12);
  g->rng = get_u32(hdr + 16);
  for (int i = 0; i < OBJ_COUNT; i++) {
    g->objects[i].status = hdr[20 + 2 * i];
    g->objects[i].hp = hdr[21 + 2 * i];
  }
  for (unsigned i = 0; i < count; i++) {
    const uint8_t *p = tw + 1 + 5 * i;
    Tower *o = &g->towers[i];
    o->active = 1; o->type = p[0]; o->col = p[1]; o->row = p[2]; o->hp = p[3]; o->ammo = p[4];
  }
  g->phase = PHASE_DAY;
  if (pending_night) *pending_night = hdr[1] != 0;
  return true;
}

size_t save_write(const Game *g, bool pending_night, uint8_t *buf, size_t cap) {
  if (cap < SAVE_LEN) return 0;
  if (!save_write_header(g, pending_night, buf, SAVE_HDR_LEN)) return 0;
  if (!save_write_towers(g, buf + SAVE_HDR_LEN, SAVE_TOWERS_LEN)) return 0;
  return SAVE_LEN;
}

bool save_read_blob(Game *g, bool *pending_night, const uint8_t *buf, size_t len) {
  if (!buf || (len != SAVE_LEN && len != SAVE_LEN_V1)) return false;
  size_t hdr_len = len - SAVE_TOWERS_LEN;
  return save_read(g, pending_night, buf, hdr_len, buf + hdr_len, SAVE_TOWERS_LEN);
}

bool best_update(Best *b, uint16_t nights, uint32_t score) {
  if (best_beats((Best){ nights, score }, *b)) {
    b->nights = nights; b->score = score;
    return true;
  }
  return false;
}
