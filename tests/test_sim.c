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
  Game g; game_new_start(&g, 3, DIFF_NORMAL, OBJ_TANK_A);
  CHECK(night_begin(&g));
  CHECK(!night_begin(&g));
  CHECK(g.phase == PHASE_NIGHT);
  CHECK(g.wave_len > 0);
  for (int i = 0; i < 3000 && g.phase == PHASE_NIGHT; i++) sim_tick(&g, 40);
  CHECK(g.phase == PHASE_REPORT);                  /* seed 3, undefended: survives night 1 */
  CHECK_EQ(g.night, 2);
  CHECK_EQ(g.stats.kills, 0);
  CHECK_EQ(g.money, DIFFICULTY_SPECS[DIFF_NORMAL].start_money + g.stats.bounty + g.stats.income);
  CHECK(g.score >= SCORE_PER_NIGHT);
}

static void test_defended_night(void) {
  Game g; game_new_start(&g, 3, DIFF_NORMAL, OBJ_TANK_A);
  int n = 0;
  n += tower_buy(&g, TW_MG, 4, 8) == ACT_OK;
  n += tower_buy(&g, TW_MG, 7, 8) == ACT_OK;
  n += tower_buy(&g, TW_MG, 10, 8) == ACT_OK;
  CHECK(n > 0);
  int32_t spent = DIFFICULTY_SPECS[DIFF_NORMAL].start_money - g.money;
  CHECK(night_begin(&g));
  for (int i = 0; i < 3000 && g.phase == PHASE_NIGHT; i++) sim_tick(&g, 40);
  CHECK(g.phase == PHASE_REPORT);
  CHECK(g.stats.kills > 0);
  CHECK_EQ(g.money, DIFFICULTY_SPECS[DIFF_NORMAL].start_money - spent + g.stats.bounty + g.stats.income);
  CHECK_EQ(g.score, (uint32_t)g.stats.bounty + SCORE_PER_NIGHT);
}

static void test_night_waits_for_air(void) {
  Game g; game_new_start(&g, 1, DIFF_NORMAL, OBJ_TANK_A);
  night_begin(&g);
  g.wave_len = 0; g.wave_next = 0;
  g.money = 100;
  CHECK(ability_use(&g, AB_AIR, at(50, 50)));
  sim_tick(&g, 100);
  CHECK(g.phase == PHASE_NIGHT);                   /* strike pending */
  for (int i = 0; i < 14; i++) sim_tick(&g, 100);
  CHECK(!g.air_active);
  CHECK(g.phase == PHASE_REPORT);
}

static void test_report_and_next_night(void) {
  Game g; game_new_start(&g, 3, DIFF_NORMAL, OBJ_TANK_A);
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
  CHECK_EQ(g.money, DIFFICULTY_SPECS[DIFF_NORMAL].start_money + g.stats.bounty + g.stats.income);
  CHECK(g.score >= SCORE_PER_NIGHT);
  CHECK(g.seed != seed1);
  sim_tick(&g, 40);
  CHECK(g.phase == PHASE_REPORT);                  /* no-op outside NIGHT */
  report_ack(&g);
  CHECK(g.phase == PHASE_DAY);
  CHECK(night_begin(&g));
  CHECK(g.wave_len != len1 || memcmp(first, g.wave, sizeof(first)) != 0);
}

static void test_air_support(void) {
  Game g; game_new_start(&g, 1, DIFF_NORMAL, OBJ_TANK_A);
  night_begin(&g);
  g.wave_len = 0; g.wave_next = 0;
  g.money = 59;
  CHECK(!ability_ready(&g, AB_AIR));
  CHECK(!ability_use(&g, AB_AIR, at(50, 50)));
  CHECK_EQ(g.money, 59);
  g.money = 100;
  CHECK(ability_use(&g, AB_AIR, at(50, 50)));
  CHECK_EQ(g.money, 40);
  CHECK(g.air_active);
  CHECK_EQ(g.ab_cooldown_ms[AB_AIR], ABILITY_SPECS[AB_AIR].cooldown_ms);
  g.money = 100;
  CHECK(!ability_use(&g, AB_AIR, at(50, 50)));     /* on cooldown */
  CHECK_EQ(g.money, 100);
  g.money = 40;
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

static void test_cracker_abilities(void) {
  Game g; game_new_start(&g, 1, DIFF_NORMAL, OBJ_CRACKER);
  night_begin(&g);
  CHECK_EQ(ability_cost(&g, AB_AIR), 42); CHECK_EQ(ability_cost(&g, AB_EW), 17); CHECK_EQ(ability_cost(&g, AB_FLARE), 10);
  CHECK_EQ(ability_cooldown(&g, AB_AIR), 17500);
  g.money = 42;
  CHECK(ability_ready(&g, AB_AIR));
  CHECK(ability_use(&g, AB_AIR, at(50, 50)));
  CHECK_EQ(g.money, 0); CHECK_EQ(g.ab_cooldown_ms[AB_AIR], 17500);
  g.objects[OBJ_CRACKER].status = OBJ_DESTROYED;              /* role gone: full price and cooldown */
  CHECK_EQ(ability_cost(&g, AB_EW), 25); CHECK_EQ(ability_cooldown(&g, AB_AIR), 25000);
  g.money = 24; CHECK(!ability_ready(&g, AB_EW));
}

static void test_air_not_in_day(void) {
  Game g; game_new_start(&g, 1, DIFF_NORMAL, OBJ_TANK_A); g.money = 500;
  for (int a = 0; a < AB_COUNT; a++) CHECK(!ability_use(&g, (Ability)a, at(50, 50)));
  CHECK_EQ(g.money, 500);
}

static void test_ability_cooldowns(void) {
  Game g; game_new_start(&g, 1, DIFF_NORMAL, OBJ_TANK_A);
  night_begin(&g);
  g.wave_len = 0; g.wave_next = 0;
  put_drone(&g, 0, DR_SHAHED136, at(10, 10), 1);   /* keeps the night going */
  g.drones[0].wps[0] = g.drones[0].wps[1] = g.drones[0].wps[2] = at(10, 11);
  g.money = 1000;
  CHECK(ability_use(&g, AB_EW, at(50, 50)));
  CHECK(ability_use(&g, AB_FLARE, at(60, 60)));
  CHECK_EQ(g.money, 1000 - ABILITY_SPECS[AB_EW].cost - ABILITY_SPECS[AB_FLARE].cost);
  CHECK_EQ(g.ew_ms, ABILITY_SPECS[AB_EW].duration_ms);
  CHECK_EQ(g.flare_ms, ABILITY_SPECS[AB_FLARE].duration_ms);
  CHECK(!ability_ready(&g, AB_EW) && !ability_ready(&g, AB_FLARE) && ability_ready(&g, AB_AIR));
  for (int a = 0; a < AB_COUNT; a++)                /* a zone never outlives its cooldown */
    CHECK(ABILITY_SPECS[a].cooldown_ms > ABILITY_SPECS[a].duration_ms);
  g.drones[0].active = 0;                           /* only the timers below matter now */
  g.wave_len = 1; g.wave_next = 0; g.wave[0].time_ms = 60000;   /* never spawns: night stays */
  for (int t = 0; t < ABILITY_SPECS[AB_FLARE].duration_ms; t += 40) sim_tick(&g, 40);
  CHECK_EQ(g.flare_ms, 0);
  CHECK(g.ew_ms == 0);                              /* 5 s < 6 s */
  CHECK(!ability_ready(&g, AB_FLARE));
  for (int t = ABILITY_SPECS[AB_FLARE].duration_ms; t < ABILITY_SPECS[AB_FLARE].cooldown_ms; t += 40) sim_tick(&g, 40);
  CHECK(ability_ready(&g, AB_FLARE));
  CHECK(!ability_ready(&g, AB_EW));
  CHECK(night_begin(&g) == false);                  /* still night */
  g.phase = PHASE_DAY;
  CHECK(night_begin(&g));                           /* a new night resets every cooldown */
  for (int a = 0; a < AB_COUNT; a++) CHECK_EQ(g.ab_cooldown_ms[a], 0);
}

static void test_game_over(void) {
  Game g; game_new_start(&g, 1, DIFF_NORMAL, OBJ_TANK_A);
  night_begin(&g);
  for (int o = 0; o < OBJ_COUNT; o++) g.objects[o].status = OBJ_DESTROYED;
  sim_tick(&g, 40);
  CHECK(g.phase == PHASE_GAME_OVER);
  CHECK_EQ(g.night, 1);
  CHECK_EQ(g.money, DIFFICULTY_SPECS[DIFF_NORMAL].start_money);
}

static void test_spawn_pool_full(void) {
  Game g; game_new_start(&g, 1, DIFF_NORMAL, OBJ_TANK_A);
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
  Game g; game_new_start(&g, 1, DIFF_NORMAL, OBJ_TANK_A);
  g.events = EV_MISSILE | EV_EXPLOSION;
  CHECK_EQ(sim_take_events(&g), EV_MISSILE | EV_EXPLOSION);
  CHECK_EQ(g.events, 0);
  CHECK_EQ(sim_take_events(&g), 0);
}

int main(void) {
  RUN(test_cracker_abilities);
  RUN(test_begin_and_full_night);
  RUN(test_defended_night);
  RUN(test_night_waits_for_air);
  RUN(test_report_and_next_night);
  RUN(test_air_support);
  RUN(test_air_not_in_day);
  RUN(test_ability_cooldowns);
  RUN(test_game_over);
  RUN(test_spawn_pool_full);
  RUN(test_take_events);
  return TEST_SUMMARY();
}
