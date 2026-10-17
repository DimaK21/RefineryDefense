#include <string.h>
#include "economy.h"
#include "config.h"
#include "rng.h"

void game_new(Game *g, uint32_t seed) {
  memset(g, 0, sizeof(*g));
  g->rng = seed ? seed : 1;
  g->seed = rng_next(&g->rng);
  g->phase = PHASE_DAY;
  g->night = 1;
  g->money = START_MONEY;
  g->objects[OBJ_DISTILLATION].status = OBJ_ALIVE;
  g->objects[OBJ_DISTILLATION].hp = OBJECT_MAX_HP;
  g->objects[OBJ_TANK_A].status = OBJ_ALIVE;
  g->objects[OBJ_TANK_A].hp = OBJECT_MAX_HP;
}

int alive_objects(const Game *g) {
  int n = 0;
  for (int i = 0; i < OBJ_COUNT; i++)
    if (g->objects[i].status == OBJ_ALIVE) n++;
  return n;
}

static bool valid_tower(const Game *g, int idx) {
  return idx >= 0 && idx < MAX_TOWERS && g->towers[idx].active;
}

int tower_at(const Game *g, int col, int row) {
  for (int i = 0; i < MAX_TOWERS; i++)
    if (g->towers[i].active && g->towers[i].col == col && g->towers[i].row == row) return i;
  return -1;
}

int object_at(const Game *g, int col, int row) {
  (void)g;
  for (int o = 0; o < OBJ_COUNT; o++) {
    const ObjectSpec *s = &OBJECT_SPECS[o];
    if (col >= s->col && col < s->col + OBJECT_CELLS && row >= s->row && row < s->row + OBJECT_CELLS) return o;
  }
  return -1;
}

bool cell_free(const Game *g, int col, int row) {
  if (col < 0 || col >= GRID_COLS || row < 0 || row >= GRID_ROWS) return false;
  return object_at(g, col, row) < 0 && tower_at(g, col, row) < 0;
}

ActResult tower_buy(Game *g, TowerType t, int col, int row) {
  if (g->phase != PHASE_DAY || (int)t >= TW_COUNT) return ACT_INVALID;
  if (!cell_free(g, col, row)) return ACT_BLOCKED;
  int slot = -1;
  for (int i = 0; i < MAX_TOWERS; i++)
    if (!g->towers[i].active) { slot = i; break; }
  if (slot < 0) return ACT_BLOCKED;
  const TowerSpec *s = &TOWER_SPECS[t];
  if (g->money < s->cost) return ACT_NO_MONEY;
  g->money -= s->cost;
  Tower *tw = &g->towers[slot];
  memset(tw, 0, sizeof(*tw));
  tw->active = 1;
  tw->type = (uint8_t)t;
  tw->col = (uint8_t)col;
  tw->row = (uint8_t)row;
  tw->hp = s->hp;
  tw->ammo = s->ammo;
  return ACT_OK;
}

ActResult tower_move(Game *g, int idx, int col, int row) {
  if (g->phase != PHASE_DAY || !valid_tower(g, idx)) return ACT_INVALID;
  if (!cell_free(g, col, row)) return ACT_BLOCKED;
  g->towers[idx].col = (uint8_t)col;
  g->towers[idx].row = (uint8_t)row;
  return ACT_OK;
}

int32_t tower_sell_value(const Game *g, int idx) {
  if (!valid_tower(g, idx)) return 0;
  return (int32_t)TOWER_SPECS[g->towers[idx].type].cost * SELL_REFUND_PCT / 100;
}

ActResult tower_sell(Game *g, int idx) {
  if (g->phase != PHASE_DAY || !valid_tower(g, idx)) return ACT_INVALID;
  g->money += tower_sell_value(g, idx);
  g->towers[idx].active = 0;
  return ACT_OK;
}

int32_t tower_repair_cost(const Game *g, int idx) {
  if (!valid_tower(g, idx)) return 0;
  const TowerSpec *s = &TOWER_SPECS[g->towers[idx].type];
  int missing = s->hp - g->towers[idx].hp;
  if (missing <= 0) return 0;
  int32_t num = (int32_t)s->cost * TOWER_REPAIR_PCT * missing;
  int32_t den = 100 * (int32_t)s->hp;
  return (num + den - 1) / den;
}

ActResult tower_repair(Game *g, int idx) {
  if (g->phase != PHASE_DAY || !valid_tower(g, idx)) return ACT_INVALID;
  int32_t cost = tower_repair_cost(g, idx);
  if (cost <= 0) return ACT_INVALID;
  if (g->money < cost) return ACT_NO_MONEY;
  g->money -= cost;
  g->towers[idx].hp = TOWER_SPECS[g->towers[idx].type].hp;
  return ACT_OK;
}

int32_t tower_reload_cost(const Game *g, int idx) {
  if (!valid_tower(g, idx)) return 0;
  const TowerSpec *s = &TOWER_SPECS[g->towers[idx].type];
  if (s->ammo == 0 || g->towers[idx].ammo >= s->ammo) return 0;
  return (int32_t)(s->ammo - g->towers[idx].ammo) * s->ammo_price;
}

ActResult tower_reload(Game *g, int idx) {
  if (g->phase != PHASE_DAY || !valid_tower(g, idx)) return ACT_INVALID;
  int32_t cost = tower_reload_cost(g, idx);
  if (cost <= 0) return ACT_INVALID;
  if (g->money < cost) return ACT_NO_MONEY;
  g->money -= cost;
  g->towers[idx].ammo = TOWER_SPECS[g->towers[idx].type].ammo;
  return ACT_OK;
}

int32_t object_action_cost(const Game *g, ObjectId o) {
  if ((int)o >= OBJ_COUNT) return 0;
  int32_t bc = OBJECT_SPECS[o].build_cost;
  switch (g->objects[o].status) {
    case OBJ_NOT_BUILT: return bc;
    case OBJ_ALIVE: return bc * (OBJECT_MAX_HP - g->objects[o].hp) / OBJECT_MAX_HP;
    case OBJ_DESTROYED: return bc * REBUILD_PCT / 100;
  }
  return 0;
}

ActResult object_action(Game *g, ObjectId o) {
  if (g->phase != PHASE_DAY || (int)o >= OBJ_COUNT) return ACT_INVALID;
  int32_t cost = object_action_cost(g, o);
  if (cost <= 0) return ACT_INVALID;
  if (g->money < cost) return ACT_NO_MONEY;
  g->money -= cost;
  g->objects[o].status = OBJ_ALIVE;
  g->objects[o].hp = OBJECT_MAX_HP;
  return ACT_OK;
}

int32_t morning_income(const Game *g) {
  int32_t sum = 0;
  for (int o = 0; o < OBJ_COUNT; o++)
    if (g->objects[o].status == OBJ_ALIVE)
      sum += (int32_t)OBJECT_SPECS[o].income * g->objects[o].hp / 100;
  return sum;
}

int plant_health_pct(const Game *g) {
  int32_t hp = 0, built = 0;
  for (int o = 0; o < OBJ_COUNT; o++) {
    if (g->objects[o].status == OBJ_NOT_BUILT) continue;
    built++;
    if (g->objects[o].status == OBJ_ALIVE) hp += g->objects[o].hp;
  }
  return built ? (int)(hp * 100 / (built * OBJECT_MAX_HP)) : 0;
}
