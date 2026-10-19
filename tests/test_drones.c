#include <string.h>
#include "test.h"
#include "config.h"
#include "economy.h"
#include "drones.h"
#include "fx.h"

static int32_t absi(int32_t v) { return v < 0 ? -v : v; }

static void test_object_hit(void) {
  Game g; game_new_start(&g, 5, DIFF_NORMAL, OBJ_TANK_A); WaveEntry e = {0, DR_SHAHED136, 0};
  CHECK(drone_spawn(&g, &e));
  CHECK(g.drones[0].active);
  CHECK_EQ(g.drones[0].uid, 1);
  CHECK_EQ(g.drones[0].hp, DRONE_SPECS[DR_SHAHED136].hp);
  CHECK_EQ(g.drones[0].track.y, 0);
  for (int i = 0; i < 2000 && g.drones[0].active; i++) drones_update(&g, 40);
  CHECK(!g.drones[0].active);
  CHECK(g.objects[OBJ_DISTILLATION].hp == 65 || g.objects[OBJ_TANK_A].hp == 65);
  CHECK(g.events & EV_OBJECT_HIT);
  CHECK(g.events & EV_EXPLOSION);
  CHECK_EQ(g.stats.objects_hit, 1);
  CHECK(g.effects[0].active && g.effects[0].kind == FX_SPAWN);   /* edge blip from drone_spawn */
  CHECK_EQ(g.effects[0].ttl_ms, SPAWN_WARN_MS);
  CHECK(g.effects[1].active && g.effects[1].kind == FX_IMPACT);
  CHECK_EQ(g.effects[1].ttl_ms, IMPACT_MS);
}

static void test_object_destroyed(void) {
  Game g; game_new_start(&g, 5, DIFF_NORMAL, OBJ_TANK_A); WaveEntry e = {0, DR_SHAHED136, 0};
  g.objects[OBJ_DISTILLATION].hp = 20; g.objects[OBJ_TANK_A].hp = 20;
  CHECK(drone_spawn(&g, &e));
  int t = g.drones[0].target_idx;
  for (int i = 0; i < 2000 && g.drones[0].active; i++) drones_update(&g, 40);
  CHECK_EQ(g.objects[t].hp, 0);
  CHECK_EQ(g.objects[t].status, OBJ_DESTROYED);
  CHECK(g.events & EV_OBJECT_DESTROYED);
}

static void test_wobble(void) {
  Game g; game_new_start(&g, 7, DIFF_NORMAL, OBJ_TANK_A); WaveEntry e = {0, DR_SHAHED136, 1};
  CHECK(drone_spawn(&g, &e));
  int differs = 0; int32_t maxoff = 0; uint8_t amp = g.drones[0].amp;
  CHECK(amp >= WOBBLE_AMP_MIN && amp <= WOBBLE_AMP_MAX);
  for (int i = 0; i < 100 && g.drones[0].active; i++) {
    drones_update(&g, 40);
    Drone *d = &g.drones[0];
    int32_t off = fx_dist(d->pos.x - d->track.x, d->pos.y - d->track.y);
    if (off > 0) differs = 1;
    if (off > maxoff) maxoff = off;
  }
  CHECK(differs);
  CHECK(maxoff > 0 && maxoff <= (amp + 1) * FX);
}

static void put_drone(Game *g, int i, int type, Vec track, Vec far) {
  Drone *d = &g->drones[i];
  memset(d, 0, sizeof(*d));
  d->active = 1; d->type = (uint8_t)type; d->hp = DRONE_SPECS[type].hp;
  d->target_kind = TGT_OBJECT; d->target_idx = OBJ_DISTILLATION;
  d->track = d->pos = track;
  d->wps[0] = d->wps[1] = d->wps[2] = far;
  d->amp = 3;
}

static void test_jammer(void) {
  Game g; game_new_start(&g, 3, DIFF_NORMAL, OBJ_TANK_A); g.money = 1000;
  CHECK_EQ(tower_buy(&g, TW_JAMMER, 1, 1), ACT_OK);          /* center (18,18) */
  Vec a = {18 * FX, 18 * FX}, b = {18 * FX, 150 * FX};
  Vec fa = {118 * FX, 18 * FX}, fb = {118 * FX, 150 * FX};
  put_drone(&g, 0, DR_SWITCHBLADE, a, fa);                   /* jammed */
  put_drone(&g, 1, DR_SWITCHBLADE, b, fb);                   /* free */
  CHECK(drone_jammed(&g, 0));
  CHECK(!drone_jammed(&g, 1));
  for (int i = 0; i < 25; i++) drones_update(&g, 40);
  int32_t da = g.drones[0].track.x - a.x, db = g.drones[1].track.x - b.x;
  CHECK(db > 13 * FX);
  CHECK(absi(da - db * 60 / 100) <= FX);
}

static void test_harpy_radar(void) {
  Game g; game_new_start(&g, 9, DIFF_NORMAL, OBJ_TANK_A); g.money = 1000;
  tower_buy(&g, TW_GEPARD, 8, 1);                            /* near the top edge */
  tower_buy(&g, TW_RADAR, 13, 17);                           /* far away */
  int radar = tower_at(&g, 13, 17);
  WaveEntry e = {0, DR_HARPY, 0};
  CHECK(drone_spawn(&g, &e));
  CHECK_EQ(g.drones[0].target_kind, TGT_TOWER);
  CHECK_EQ(g.drones[0].target_idx, radar);
  Vec c = tower_center(&g.towers[radar]);
  CHECK_EQ(g.drones[0].wps[2].x, c.x);
  CHECK_EQ(g.drones[0].wps[2].y, c.y);
}

static void test_harpy_fallbacks(void) {
  Game g; game_new_start(&g, 9, DIFF_NORMAL, OBJ_TANK_A); g.money = 1000;
  tower_buy(&g, TW_MG, 8, 1);
  WaveEntry e = {0, DR_HARPY, 0};
  CHECK(drone_spawn(&g, &e));
  CHECK_EQ(g.drones[0].target_kind, TGT_TOWER);              /* no radar: nearest tower */
  Game h; game_new_start(&h, 9, DIFF_NORMAL, OBJ_TANK_A);
  CHECK(drone_spawn(&h, &e));
  CHECK_EQ(h.drones[0].target_kind, TGT_OBJECT);             /* no towers: object */
}

static void test_pool(void) {
  Game g2; game_new_start(&g2, 11, DIFF_NORMAL, OBJ_TANK_A); WaveEntry e = {0, DR_SHAHED136, 0};
  for (int i = 0; i < MAX_DRONES; i++) CHECK(drone_spawn(&g2, &e));
  Game before = g2;
  CHECK(!drone_spawn(&g2, &e));
  CHECK(memcmp(&before, &g2, sizeof(Game)) == 0);
  CHECK_EQ(g2.drones[MAX_DRONES - 1].uid, MAX_DRONES);
}

static void test_no_target(void) {
  Game g; game_new_start(&g, 2, DIFF_NORMAL, OBJ_TANK_A); WaveEntry e = {0, DR_SHAHED136, 2};
  g.objects[OBJ_DISTILLATION].status = OBJ_DESTROYED;
  g.objects[OBJ_TANK_A].status = OBJ_DESTROYED;
  CHECK(drone_spawn(&g, &e));
  for (int i = 0; i < MAX_DRONES; i++) CHECK(!g.drones[i].active);
}

static void test_retarget(void) {
  Game g; game_new_start(&g, 4, DIFF_NORMAL, OBJ_TANK_A); g.money = 1000;
  tower_buy(&g, TW_MG, 0, 0);
  Vec start = {100 * FX, 0}, far = tower_center(&g.towers[0]);
  put_drone(&g, 0, DR_SWITCHBLADE, start, far);
  g.drones[0].target_kind = TGT_TOWER; g.drones[0].target_idx = 0;
  tower_sell(&g, 0);
  drones_update(&g, 40);
  Drone *d = &g.drones[0];
  CHECK(d->active);
  CHECK_EQ(d->target_kind, TGT_OBJECT);
  Vec c = object_center((ObjectId)d->target_idx);
  CHECK(g.objects[d->target_idx].status == OBJ_ALIVE);
  CHECK_EQ(d->wps[2].x, c.x); CHECK_EQ(d->wps[2].y, c.y);
  /* no living objects: drone disappears */
  g.objects[OBJ_DISTILLATION].status = OBJ_DESTROYED;
  g.objects[OBJ_TANK_A].status = OBJ_DESTROYED;
  drones_update(&g, 40);
  CHECK(!g.drones[0].active);
}

static void test_tower_lost(void) {
  Game g; game_new_start(&g, 4, DIFF_NORMAL, OBJ_TANK_A); g.money = 1000;
  tower_buy(&g, TW_MG, 5, 5);
  Vec c = tower_center(&g.towers[0]);
  g.towers[0].hp = 1;
  put_drone(&g, 0, DR_SWITCHBLADE, c, c);
  g.drones[0].target_kind = TGT_TOWER; g.drones[0].target_idx = 0; g.drones[0].wp_idx = 2;
  drones_update(&g, 40);
  CHECK(!g.drones[0].active);
  CHECK(!g.towers[0].active);
  CHECK(g.events & EV_TOWER_LOST);
  CHECK_EQ(g.stats.towers_lost, 1);
}

static void test_centers(void) {
  Tower t = {1, TW_MG, 2, 3, 3, 0, 0};
  Vec c = tower_center(&t);
  CHECK_EQ(c.x, (2 * CELL + CELL / 2) * FX); CHECK_EQ(c.y, (3 * CELL + CELL / 2) * FX);
  Vec o = object_center(OBJ_TANK_A);
  CHECK_EQ(o.x, (3 * CELL + CELL) * FX); CHECK_EQ(o.y, (12 * CELL + CELL) * FX);
}

static void test_ew_pulse_slows(void) {
  Game g; game_new_start(&g, 5, DIFF_NORMAL, OBJ_TANK_A); WaveEntry e = {0, DR_SHAHED136, 0};
  CHECK(drone_spawn(&g, &e));
  Game h = g;
  h.ew_point = h.drones[0].track; h.ew_ms = 30000;           /* pulse right on the drone */
  Vec a0 = g.drones[0].track;
  for (int i = 0; i < 25; i++) { drones_update(&g, 40); drones_update(&h, 40); }
  int32_t free_d = fx_dist(g.drones[0].track.x - a0.x, g.drones[0].track.y - a0.y);
  int32_t slow_d = fx_dist(h.drones[0].track.x - a0.x, h.drones[0].track.y - a0.y);
  /* ~40% of the free flight; per-tick step truncation costs a little */
  CHECK(slow_d * 100 >= free_d * (100 - EW_SLOW_PCT - 3));
  CHECK(slow_d * 100 <= free_d * (100 - EW_SLOW_PCT + 3));
}

int main(void) {
  RUN(test_ew_pulse_slows);
  RUN(test_object_hit);
  RUN(test_object_destroyed);
  RUN(test_wobble);
  RUN(test_jammer);
  RUN(test_harpy_radar);
  RUN(test_harpy_fallbacks);
  RUN(test_pool);
  RUN(test_no_target);
  RUN(test_retarget);
  RUN(test_tower_lost);
  RUN(test_centers);
  return TEST_SUMMARY();
}
