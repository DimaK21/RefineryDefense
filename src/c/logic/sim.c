#include <string.h>
#include "sim.h"
#include "config.h"
#include "economy.h"
#include "waves.h"
#include "drones.h"
#include "combat.h"
#include "fx.h"
#include "rng.h"

bool night_begin(Game *g) {
  if (g->phase != PHASE_DAY) return false;
  memset(g->drones, 0, sizeof(g->drones));
  memset(g->missiles, 0, sizeof(g->missiles));
  memset(g->effects, 0, sizeof(g->effects));
  memset(&g->stats, 0, sizeof(g->stats));
  g->wave_len = wave_generate(g->wave, MAX_WAVE, g->night, alive_objects(g), g->seed);
  g->wave_next = 0; g->night_ms = 0;
  g->air_used = 0; g->air_active = 0; g->air_timer_ms = 0;
  g->events = 0;
  g->phase = PHASE_NIGHT;
  return true;
}

bool air_support_call(Game *g, Vec p) {
  if (g->phase != PHASE_NIGHT || g->air_used || g->money < AIR_COST) return false;
  g->money -= AIR_COST;
  g->air_used = 1; g->air_active = 1;
  g->air_point = p; g->air_timer_ms = AIR_DELAY_MS;
  return true;
}

static void air_strike(Game *g) {
  int32_t r = AIR_RADIUS * FX;
  for (int i = 0; i < MAX_DRONES; i++) {
    const Drone *d = &g->drones[i];
    if (!d->active) continue;
    if (fx_dist(d->pos.x - g->air_point.x, d->pos.y - g->air_point.y) <= r) kill_drone(g, i);
  }
  effect_add(g, FX_AIRSTRIKE, g->air_point, g->air_point, EFFECT_MS);
  g->air_active = 0;
}

void sim_tick(Game *g, uint16_t dt_ms) {
  if (g->phase != PHASE_NIGHT) return;
  g->night_ms += dt_ms;
  while (g->wave_next < g->wave_len && g->wave[g->wave_next].time_ms <= g->night_ms) {
    if (!drone_spawn(g, &g->wave[g->wave_next])) break;
    g->wave_next++;
  }
  drones_update(g, dt_ms);
  combat_update(g, dt_ms);
  if (g->air_active) {
    g->air_timer_ms = (int16_t)(g->air_timer_ms - dt_ms);
    if (g->air_timer_ms <= 0) air_strike(g);
  }
  for (int i = 0; i < MAX_EFFECTS; i++) {
    Effect *e = &g->effects[i];
    if (!e->active) continue;
    e->ttl_ms = (int16_t)(e->ttl_ms - dt_ms);
    if (e->ttl_ms <= 0) e->active = 0;
  }
  if (alive_objects(g) == 0) { g->phase = PHASE_GAME_OVER; return; }
  if (g->wave_next < g->wave_len) return;
  for (int i = 0; i < MAX_DRONES; i++) if (g->drones[i].active) return;
  g->stats.income = morning_income(g);
  g->money += g->stats.income;
  g->score += 100;
  g->night++;
  g->seed = rng_next(&g->rng);
  g->phase = PHASE_REPORT;
}

uint32_t sim_take_events(Game *g) {
  uint32_t e = g->events;
  g->events = 0;
  return e;
}

uint16_t nights_survived(const Game *g) { return (uint16_t)(g->night - 1); }

void report_ack(Game *g) {
  if (g->phase == PHASE_REPORT) g->phase = PHASE_DAY;
}
