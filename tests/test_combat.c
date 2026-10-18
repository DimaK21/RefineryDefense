#include <string.h>
#include "test.h"
#include "config.h"
#include "economy.h"
#include "drones.h"
#include "combat.h"
#include "fx.h"

static void put_drone(Game *g, int i, int type, Vec pos, Vec dest, uint16_t uid) {
  Drone *d = &g->drones[i];
  memset(d, 0, sizeof(*d));
  d->active = 1; d->type = (uint8_t)type; d->hp = DRONE_SPECS[type].hp; d->uid = uid;
  d->target_kind = TGT_OBJECT; d->target_idx = OBJ_DISTILLATION;
  d->track = d->pos = pos;
  d->wps[0] = d->wps[1] = d->wps[2] = dest;
}

static Vec at(int x, int y) { Vec v = {x * FX, y * FX}; return v; }

static void test_engage_range(void) {
  Game g; game_new(&g, 1); g.money = 1000;
  CHECK_EQ(tower_buy(&g, TW_MG, 5, 5), ACT_OK);             /* center (66,66) */
  CHECK_EQ(tower_engage_range(&g, 0), 13 * FX);
  CHECK_EQ(tower_buy(&g, TW_RADAR, 6, 5), ACT_OK);
  CHECK_EQ(tower_engage_range(&g, 0), 22 * FX);
  Game h; game_new(&h, 1); h.money = 1000;
  tower_buy(&h, TW_PATRIOT, 5, 5);
  CHECK_EQ(tower_engage_range(&h, 0), 48 * FX);             /* 80*60/100 */
}

static void test_mg_kills_switchblade(void) {
  Game g; game_new(&g, 1); g.money = 1000;
  tower_buy(&g, TW_MG, 5, 5);
  g.money = 0;
  put_drone(&g, 0, DR_SWITCHBLADE, at(70, 66), at(70, 200), 1);
  for (int i = 0; i < 5; i++) combat_update(&g, 50);        /* 250 ms: one shot */
  CHECK(g.drones[0].active);
  CHECK_EQ(g.drones[0].hp, 1);
  CHECK(g.effects[0].active && g.effects[0].kind == FX_TRACER);
  CHECK_EQ(g.effects[0].ttl_ms, TRACER_MS);
  combat_update(&g, 50);
  CHECK(!g.drones[0].active);
  CHECK_EQ(g.money, 3);
  CHECK_EQ(g.stats.kills, 1);
  CHECK_EQ(g.stats.bounty, 3);
  CHECK_EQ(g.score, 3);
  CHECK(g.events & EV_EXPLOSION);
  int bounty_fx = 0;
  for (int i = 0; i < MAX_EFFECTS; i++)
    if (g.effects[i].active && g.effects[i].kind == FX_BOUNTY) {
      bounty_fx++;
      CHECK_EQ(g.effects[i].value, 3);
      CHECK_EQ(g.effects[i].ttl_ms, BOUNTY_MS);
    }
  CHECK_EQ(bounty_fx, 1);
}

static void test_visibility(void) {
  Game g; game_new(&g, 1); g.money = 1000;
  put_drone(&g, 0, DR_GERBERA, at(100, 92), at(100, 200), 1);
  CHECK(!drone_visible(&g, 0));
  tower_buy(&g, TW_MG, 8, 7);                               /* center (102,90) */
  CHECK(drone_visible(&g, 0));                              /* inside engage range */
  CHECK(!drone_identified(&g, 0));                          /* no radar */
  put_drone(&g, 1, DR_SWITCHBLADE, at(130, 90), at(130, 200), 2);
  CHECK(!drone_visible(&g, 1));
  tower_buy(&g, TW_RADAR, 9, 7);
  CHECK(drone_visible(&g, 1));                              /* radar range 70 */
  CHECK(drone_identified(&g, 1));
  CHECK(drone_identified(&g, 0));
  put_drone(&g, 2, DR_SWITCHBLADE, at(190, 220), at(190, 221), 3);
  CHECK(!drone_visible(&g, 2));
  CHECK(!drone_identified(&g, 2));
}

static void test_range_boundary(void) {
  Game g; game_new(&g, 1); g.money = 1000;
  tower_buy(&g, TW_MG, 5, 5);                               /* center (66,66), engage 13 */
  Vec c = at(66, 66);
  Vec p = {c.x + 13 * FX, c.y + 1};                         /* floor dist == 13*FX: in range */
  put_drone(&g, 0, DR_SWITCHBLADE, p, at(70, 200), 1);
  CHECK(drone_visible(&g, 0));
  p.x += 1;                                                 /* one Q8 step further: out */
  put_drone(&g, 1, DR_SWITCHBLADE, p, at(70, 200), 2);
  CHECK(!drone_visible(&g, 1));
  combat_update(&g, 40);
  CHECK(g.drones[0].hp < DRONE_SPECS[DR_SWITCHBLADE].hp);   /* the in-range one was shot */
  CHECK_EQ(g.drones[1].hp, DRONE_SPECS[DR_SWITCHBLADE].hp);
}

static void test_engage_cache(void) {
  Game g; game_new(&g, 1); g.money = 100000;
  CHECK_EQ(tower_buy(&g, TW_MG, 5, 5), ACT_OK);
  CHECK_EQ(tower_buy(&g, TW_RADAR, 6, 5), ACT_OK);
  CHECK_EQ(tower_buy(&g, TW_PATRIOT, 14, 17), ACT_OK);     /* far from the radar */
  CHECK_EQ(tower_buy(&g, TW_JAMMER, 1, 17), ACT_OK);       /* not firing */
  int32_t r[MAX_TOWERS];
  combat_engage_ranges(&g, r);
  CHECK_EQ(r[0], 22 * FX);
  CHECK_EQ(r[1], 0);                                        /* radar does not fire */
  CHECK_EQ(r[2], 48 * FX);
  CHECK_EQ(r[3], 0);
  for (int i = 4; i < MAX_TOWERS; i++) CHECK_EQ(r[i], 0);
  for (int i = 0; i < MAX_DRONES; i++)
    put_drone(&g, i, i % DR_COUNT, at(4 + (i * 37) % 192, 4 + (i * 53) % 220), at(100, 100), (uint16_t)(i + 1));
  int vis = 0;
  for (int i = 0; i < MAX_DRONES; i++) {
    CHECK_EQ(drone_visible_in(&g, i, r), drone_visible(&g, i));
    vis += drone_visible(&g, i);
  }
  CHECK(vis > 0 && vis < MAX_DRONES);                       /* the sample covers both cases */
}

static void test_tor_gerbera(void) {
  Game g; game_new(&g, 1); g.money = 1000;
  tower_buy(&g, TW_TOR, 5, 5);
  tower_buy(&g, TW_RADAR, 6, 6);
  put_drone(&g, 0, DR_GERBERA, at(80, 66), at(80, 200), 1);
  for (int i = 0; i < 10; i++) combat_update(&g, 500);
  CHECK_EQ(g.towers[0].ammo, 6);
  for (int i = 0; i < MAX_MISSILES; i++) CHECK(!g.missiles[i].active);
  Game h; game_new(&h, 1); h.money = 1000;
  tower_buy(&h, TW_TOR, 5, 5);
  put_drone(&h, 0, DR_GERBERA, at(80, 66), at(80, 200), 1);
  combat_update(&h, 40);
  CHECK_EQ(h.towers[0].ammo, 5);
  CHECK(h.missiles[0].active);
  CHECK(h.events & EV_MISSILE);
}

static void test_patriot_ammo(void) {
  Game g; game_new(&g, 1); g.money = 1000;
  tower_buy(&g, TW_PATRIOT, 5, 5);
  put_drone(&g, 0, DR_SHAHED136, at(70, 66), at(70, 200), 1);
  g.drones[0].hp = 200;                                     /* survives */
  int launches = 0;
  for (int i = 0; i < 20; i++) {
    uint8_t before = g.towers[0].ammo;
    combat_update(&g, 4000);
    if (g.towers[0].ammo < before) launches++;
  }
  CHECK_EQ(launches, 3);
  CHECK_EQ(g.towers[0].ammo, 0);
}

static void test_missile_hit_and_stale(void) {
  Game g; game_new(&g, 1); g.money = 1000;
  tower_buy(&g, TW_TOR, 5, 5);
  put_drone(&g, 0, DR_SHAHED136, at(86, 66), at(86, 200), 7);
  combat_update(&g, 40);
  CHECK(g.missiles[0].active);
  g.drones[0].active = 0;                                   /* died, slot reused */
  put_drone(&g, 0, DR_SHAHED136, at(86, 66), at(86, 200), 8);
  uint8_t hp = g.drones[0].hp;
  combat_update(&g, 40);
  CHECK(!g.missiles[0].active);
  CHECK_EQ(g.drones[0].hp, hp);                             /* untouched */
  /* real hit: Tor damage 6 kills Shahed-136 (hp 6) */
  Game h; game_new(&h, 1); h.money = 1000;
  tower_buy(&h, TW_TOR, 5, 5);
  put_drone(&h, 0, DR_SHAHED136, at(86, 66), at(86, 200), 7);
  h.money = 0;
  for (int i = 0; i < 40 && h.drones[0].active; i++) combat_update(&h, 40);
  CHECK(!h.drones[0].active);
  CHECK_EQ(h.money, 8);
}

static void test_target_choice(void) {
  Game g; game_new(&g, 1); g.money = 1000;
  tower_buy(&g, TW_MG, 5, 5);                               /* center (66,66) */
  put_drone(&g, 0, DR_SHAHED136, at(70, 66), at(70, 150), 1);   /* far from goal */
  put_drone(&g, 1, DR_SHAHED136, at(66, 72), at(66, 80), 2);    /* close to goal */
  combat_update(&g, 40);
  CHECK_EQ(g.drones[0].hp, 6);
  CHECK_EQ(g.drones[1].hp, 5);
}

static void test_pool_full(void) {
  Game g; game_new(&g, 1); g.money = 1000;
  tower_buy(&g, TW_TOR, 5, 5);
  for (int i = 0; i < MAX_MISSILES; i++) { g.missiles[i].active = 1; g.missiles[i].drone_uid = 1; }
  put_drone(&g, 0, DR_SHAHED136, at(86, 66), at(86, 200), 1);
  combat_update(&g, 40);
  CHECK_EQ(g.towers[0].ammo, 6);
  CHECK_EQ(g.towers[0].cooldown_ms, 0);
}

static void test_flare_detects(void) {
  Game g; game_new(&g, 1); g.money = 1000;
  tower_buy(&g, TW_MG, 8, 7);                               /* center (102,90), no radar */
  CHECK_EQ(tower_engage_range(&g, 0), 22 * DETECT_PCT / 100 * FX);
  put_drone(&g, 0, DR_GERBERA, at(140, 90), at(140, 200), 1);
  CHECK(!drone_visible(&g, 0));
  g.flare_point = at(120, 90); g.flare_ms = 1000;
  CHECK(drone_visible(&g, 0));                              /* lit by the flare */
  CHECK(drone_identified(&g, 0));
  CHECK_EQ(tower_engage_range(&g, 0), 22 * FX);             /* tower inside the flare: full range */
  g.flare_ms = 0;
  CHECK(!drone_visible(&g, 0));
}

static void test_combo(void) {
  Game g; game_new(&g, 1);
  CHECK_EQ(combo_pct(1), 100); CHECK_EQ(combo_pct(COMBO_X15_AT), 150); CHECK_EQ(combo_pct(COMBO_X2_AT), 200);
  int32_t paid = 0;
  for (int k = 0; k < 7; k++) {                             /* 7 Harpies ($10) in a row */
    put_drone(&g, k, DR_HARPY, at(50, 50), at(50, 200), (uint16_t)(k + 1));
    kill_drone(&g, k);
    paid += 10 * combo_pct((uint16_t)(k + 1)) / 100;
  }
  CHECK_EQ(paid, 10 + 10 + 15 + 15 + 15 + 20 + 20);
  CHECK_EQ(g.money, START_MONEY + paid);
  CHECK_EQ(g.stats.bounty, paid);
  CHECK_EQ(g.score, (uint32_t)paid);
  CHECK_EQ(g.combo, 7);
  CHECK_EQ(g.stats.best_combo, 7);
  int last = -1;
  for (int i = 0; i < MAX_EFFECTS; i++)
    if (g.effects[i].active && g.effects[i].kind == FX_BOUNTY) last = g.effects[i].value;
  CHECK_EQ(last, 20);                                       /* popup shows the multiplied bounty */
  g.combo_ms = 0;                                           /* window expired */
  put_drone(&g, 0, DR_HARPY, at(50, 50), at(50, 200), 99);
  kill_drone(&g, 0);
  CHECK_EQ(g.combo, 1);
  CHECK_EQ(g.stats.best_combo, 7);
  CHECK_EQ(g.combo_ms, COMBO_WINDOW_MS);
}

int main(void) {
  RUN(test_flare_detects);
  RUN(test_combo);
  RUN(test_engage_range);
  RUN(test_mg_kills_switchblade);
  RUN(test_visibility);
  RUN(test_range_boundary);
  RUN(test_engage_cache);
  RUN(test_tor_gerbera);
  RUN(test_patriot_ammo);
  RUN(test_missile_hit_and_stale);
  RUN(test_target_choice);
  RUN(test_pool_full);
  return TEST_SUMMARY();
}
