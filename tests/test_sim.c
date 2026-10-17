#include <string.h>
#include "test.h"
#include "config.h"
#include "economy.h"
#include "drones.h"
#include "combat.h"
#include "sim.h"
#include "fx.h"

static Vec at(int x, int y) { Vec v = {x * FX, y * FX}; return v; }

static void put_drone(Game *g, int i, int type, Vec pos, uint16_t uid) {
  Drone *d = &g->drones[i];
  memset(d, 0, sizeof(*d));
  d->active = 1; d->type = (uint8_t)type; d->hp = DRONE_SPECS[type].hp; d->uid = uid;
  d->target_kind = TGT_OBJECT; d->target_idx = OBJ_DISTILLATION;
  d->track = d->pos = pos;
  d->wps[0] = d->wps[1] = d->wps[2] = at(100, 220);
}

static void test_begin_and_full_night(void) {
  Game g; game_new(&g, 3);
  CHECK(night_begin(&g));
  CHECK(!night_begin(&g));
  CHECK(g.phase == PHASE_NIGHT);
  CHECK(g.wave_len > 0);
  for (int i = 0; i < 3000 && g.phase == PHASE_NIGHT; i++) sim_tick(&g, 40);
  CHECK(g.phase == PHASE_REPORT || g.phase == PHASE_GAME_OVER);
  if (g.phase == PHASE_REPORT) {
    CHECK_EQ(g.night, 2);
    CHECK_EQ(g.money, 150 + g.stats.bounty + g.stats.income);
    CHECK(g.score >= 100);
  }
}

static void test_report_and_next_night(void) {
  Game g; game_new(&g, 3);
  night_begin(&g);
  uint32_t seed1 = g.seed;
  WaveEntry first[MAX_WAVE]; uint16_t len1 = g.wave_len;
  memcpy(first, g.wave, sizeof(first));
  g.wave_len = 0;                                  /* empty wave: night ends at once */
  sim_tick(&g, 40);
  CHECK(g.phase == PHASE_REPORT);
  CHECK_EQ(g.night, 2);
  CHECK_EQ(nights_survived(&g), 1);
  CHECK_EQ(g.stats.income, morning_income(&g));
  CHECK_EQ(g.money, 150 + g.stats.bounty + g.stats.income);
  CHECK(g.score >= 100);
  CHECK(g.seed != seed1);
  sim_tick(&g, 40);
  CHECK(g.phase == PHASE_REPORT);                  /* no-op outside NIGHT */
  report_ack(&g);
  CHECK(g.phase == PHASE_DAY);
  CHECK(night_begin(&g));
  CHECK(g.wave_len != len1 || memcmp(first, g.wave, sizeof(first)) != 0);
}

static void test_air_support(void) {
  Game g; game_new(&g, 1);
  night_begin(&g);
  g.wave_len = 0; g.wave_next = 0;
  g.money = 59;
  CHECK(!air_support_call(&g, at(50, 50)));
  CHECK_EQ(g.money, 59);
  g.money = 100;
  CHECK(air_support_call(&g, at(50, 50)));
  CHECK_EQ(g.money, 40);
  CHECK(g.air_used && g.air_active);
  CHECK(!air_support_call(&g, at(50, 50)));
  CHECK_EQ(g.money, 40);
  put_drone(&g, 0, DR_SHAHED136, at(60, 50), 1);   /* 10 away: dies */
  put_drone(&g, 1, DR_SHAHED136, at(50, 80), 2);   /* 30 away: lives */
  for (int i = 0; i < 14; i++) sim_tick(&g, 100);
  CHECK(g.drones[0].active && g.air_active);
  sim_tick(&g, 100);
  CHECK(!g.air_active);
  CHECK(!g.drones[0].active);
  CHECK(g.drones[1].active);
  CHECK_EQ(g.stats.kills, 1);
  CHECK(g.money > 40);
  bool fx = false;
  for (int i = 0; i < MAX_EFFECTS; i++) if (g.effects[i].active && g.effects[i].kind == FX_AIRSTRIKE) fx = true;
  CHECK(fx);
  CHECK(g.phase == PHASE_NIGHT);                   /* drone 1 still flying */
}

static void test_air_not_in_day(void) {
  Game g; game_new(&g, 1); g.money = 500;
  CHECK(!air_support_call(&g, at(50, 50)));
  CHECK_EQ(g.money, 500);
}

static void test_game_over(void) {
  Game g; game_new(&g, 1);
  night_begin(&g);
  for (int o = 0; o < OBJ_COUNT; o++) g.objects[o].status = OBJ_DESTROYED;
  sim_tick(&g, 40);
  CHECK(g.phase == PHASE_GAME_OVER);
  CHECK_EQ(g.night, 1);
  CHECK_EQ(g.money, START_MONEY);
}

static void test_spawn_pool_full(void) {
  Game g; game_new(&g, 1);
  night_begin(&g);
  g.wave_len = 1; g.wave_next = 0;
  g.wave[0].time_ms = 0; g.wave[0].type = DR_SHAHED136; g.wave[0].side = 0;
  for (int i = 0; i < MAX_DRONES; i++) put_drone(&g, i, DR_SHAHED136, at(10, 10), (uint16_t)(i + 1));
  sim_tick(&g, 40);
  CHECK_EQ(g.wave_next, 0);
  g.drones[5].active = 0;
  sim_tick(&g, 40);
  CHECK_EQ(g.wave_next, 1);
}

static void test_take_events(void) {
  Game g; game_new(&g, 1);
  g.events = EV_MISSILE | EV_EXPLOSION;
  CHECK_EQ(sim_take_events(&g), EV_MISSILE | EV_EXPLOSION);
  CHECK_EQ(g.events, 0);
  CHECK_EQ(sim_take_events(&g), 0);
}

int main(void) {
  RUN(test_begin_and_full_night);
  RUN(test_report_and_next_night);
  RUN(test_air_support);
  RUN(test_air_not_in_day);
  RUN(test_game_over);
  RUN(test_spawn_pool_full);
  RUN(test_take_events);
  return TEST_SUMMARY();
}
