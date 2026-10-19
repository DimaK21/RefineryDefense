#include <string.h>
#include "economy.h"
#include "config.h"
#include "rng.h"

void game_new_start(Game *g, uint32_t seed, Difficulty d, ObjectId second) {
  memset(g, 0, sizeof(*g));
  g->rng = seed ? seed : 1;
  g->seed = rng_next(&g->rng);
  g->phase = PHASE_DAY;
  g->night = 1;
  g->difficulty = (uint8_t)(d < DIFF_COUNT ? d : DIFF_NORMAL);
  g->money = DIFFICULTY_SPECS[g->difficulty].start_money;
  g->objects[OBJ_DISTILLATION].status = OBJ_ALIVE;
  g->objects[OBJ_DISTILLATION].hp = OBJECT_MAX_HP;
  g->objects[second].status = OBJ_ALIVE;
  g->objects[second].hp = OBJECT_MAX_HP;
}

void game_new(Game *g, uint32_t seed, Difficulty d) {
  game_new_start(g, seed, d, OBJ_TANK_A);
  g->objects[OBJ_TANK_A].status = OBJ_NOT_BUILT;
  g->objects[OBJ_TANK_A].hp = 0;
  ObjectId second = (ObjectId)rng_range(&g->rng, OBJ_TANK_A, OBJ_COUNT - 1);
  g->objects[second].status = OBJ_ALIVE;
  g->objects[second].hp = OBJECT_MAX_HP;
}

bool object_alive(const Game *g, ObjectId o) {
  return (int)o < OBJ_COUNT && g->objects[o].status == OBJ_ALIVE;
}

int32_t discounted(int32_t cost, int off_pct) {
  if (cost <= 0) return cost;
  int32_t v = cost * (100 - off_pct) / 100;
  return v < 1 ? 1 : v;
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

int32_t tower_price(const Game *g, TowerType t) {
  int32_t c = TOWER_SPECS[t].cost;
  return object_alive(g, OBJ_ROLE_TOWERS) ? discounted(c, ROLE_TOWER_OFF_PCT) : c;
}

int32_t missile_price(const Game *g, TowerType t) {
  int32_t c = TOWER_SPECS[t].ammo_price;
  return object_alive(g, OBJ_ROLE_MISSILES) ? discounted(c, ROLE_MISSILE_OFF_PCT) : c;
}

ActResult tower_buy(Game *g, TowerType t, int col, int row) {
  if (g->phase != PHASE_DAY || (int)t >= TW_COUNT) return ACT_INVALID;
  if (!cell_free(g, col, row)) return ACT_BLOCKED;
  int slot = -1;
  for (int i = 0; i < MAX_TOWERS; i++)
    if (!g->towers[i].active) { slot = i; break; }
  if (slot < 0) return ACT_BLOCKED;
  const TowerSpec *s = &TOWER_SPECS[t];
  int32_t price = tower_price(g, t);
  if (g->money < price) return ACT_NO_MONEY;
  g->money -= price;
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
  int32_t cost = (num + den - 1) / den;
  return object_alive(g, OBJ_ROLE_TOWERS) ? discounted(cost, ROLE_TOWER_OFF_PCT) : cost;
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
  return (int32_t)(s->ammo - g->towers[idx].ammo) * missile_price(g, (TowerType)g->towers[idx].type);
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

int32_t reload_all_cost(const Game *g) {
  int32_t sum = 0;
  for (int i = 0; i < MAX_TOWERS; i++) sum += tower_reload_cost(g, i);
  return sum;
}

/* Tower with the lowest ammo share that lacks missiles and whose missile is affordable, -1 if none. */
static int emptiest_tower(const Game *g) {
  int best = -1;
  for (int i = 0; i < MAX_TOWERS; i++) {
    if (tower_reload_cost(g, i) <= 0 || missile_price(g, (TowerType)g->towers[i].type) > g->money) continue;
    const Tower *t = &g->towers[i];
    if (best < 0) { best = i; continue; }
    const Tower *b = &g->towers[best];
    if ((int)t->ammo * TOWER_SPECS[b->type].ammo < (int)b->ammo * TOWER_SPECS[t->type].ammo) best = i;
  }
  return best;
}

ActResult reload_all(Game *g) {
  if (g->phase != PHASE_DAY || reload_all_cost(g) <= 0) return ACT_INVALID;
  bool bought = false;
  for (int i; (i = emptiest_tower(g)) >= 0; ) {
    g->money -= missile_price(g, (TowerType)g->towers[i].type);
    g->towers[i].ammo++;
    bought = true;
  }
  return bought ? ACT_OK : ACT_NO_MONEY;
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
