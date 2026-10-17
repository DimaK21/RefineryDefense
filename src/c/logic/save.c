#include <string.h>
#include "save.h"
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
  if (!g || !hdr || !tw || hdr_len != SAVE_HDR_LEN || tw_len != SAVE_TOWERS_LEN) return false;
  if (hdr[0] != SAVE_VERSION || hdr[1] > 1) return false;
  Game t;
  memset(&t, 0, sizeof(t));
  t.night = get_u16(hdr + 2);
  if (t.night < 1) return false;
  t.money = (int32_t)get_u32(hdr + 4);
  t.score = get_u32(hdr + 8);
  t.seed = get_u32(hdr + 12);
  t.rng = get_u32(hdr + 16);
  for (int i = 0; i < OBJ_COUNT; i++) {
    uint8_t st = hdr[20 + 2 * i], hp = hdr[21 + 2 * i];
    if (st > OBJ_DESTROYED || hp > OBJECT_MAX_HP) return false;
    if (st == OBJ_ALIVE && hp == 0) return false;
    t.objects[i].status = st; t.objects[i].hp = hp;
  }
  unsigned count = tw[0];
  if (count > MAX_TOWERS) return false;
  for (unsigned i = 0; i < count; i++) {
    const uint8_t *p = tw + 1 + 5 * i;
    if (p[0] >= TW_COUNT || p[1] >= GRID_COLS || p[2] >= GRID_ROWS) return false;
    const TowerSpec *s = &TOWER_SPECS[p[0]];
    if (p[3] > s->hp || p[4] > s->ammo) return false;
    if (object_at(&t, p[1], p[2]) >= 0) return false;
    for (unsigned j = 0; j < i; j++)
      if (t.towers[j].col == p[1] && t.towers[j].row == p[2]) return false;
    Tower *o = &t.towers[i];
    o->active = 1; o->type = p[0]; o->col = p[1]; o->row = p[2]; o->hp = p[3]; o->ammo = p[4];
  }
  t.phase = PHASE_DAY;
  *g = t;
  if (pending_night) *pending_night = hdr[1] != 0;
  return true;
}

bool best_update(Best *b, uint16_t nights, uint32_t score) {
  if (nights > b->nights || (nights == b->nights && score > b->score)) {
    b->nights = nights; b->score = score;
    return true;
  }
  return false;
}
