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
  g->wave_len = wave_generate(g->wave, MAX_WAVE, g->night, alive_objects(g), (Difficulty)g->difficulty, g->seed);
  g->wave_next = 0; g->night_ms = 0;
  memset(g->ab_cooldown_ms, 0, sizeof(g->ab_cooldown_ms));
  g->ew_ms = g->flare_ms = 0;
  g->combo = 0; g->combo_ms = 0;
  g->events = 0;
  g->phase = PHASE_NIGHT;
  return true;
}

int32_t ability_cost(const Game *g, Ability a) {
  int32_t c = ABILITY_SPECS[a].cost;
  return object_alive(g, OBJ_ROLE_ABILITIES) ? discounted(c, ROLE_ABILITY_OFF_PCT) : c;
}

uint16_t ability_cooldown(const Game *g, Ability a) {
  uint16_t c = ABILITY_SPECS[a].cooldown_ms;
  return object_alive(g, OBJ_ROLE_ABILITIES) ? (uint16_t)((uint32_t)c * (100 - ROLE_ABILITY_OFF_PCT) / 100) : c;
}

bool ability_ready(const Game *g, Ability a) {
  return g->phase == PHASE_NIGHT && a < AB_COUNT && g->ab_cooldown_ms[a] == 0
      && g->money >= ability_cost(g, a);
}

/* Every drone in the circle dies at once: a delay made the player lead moving drones blind. */
static void air_strike(Game *g, Vec p) {
  int32_t r = ABILITY_SPECS[AB_AIR].radius * FX;
  for (int i = 0; i < MAX_DRONES; i++) {
    const Drone *d = &g->drones[i];
    if (!d->active) continue;
    if (fx_within(d->pos.x - p.x, d->pos.y - p.y, r)) kill_drone(g, i);
  }
  effect_add(g, FX_AIRSTRIKE, p, p, EFFECT_MS);
}

bool ability_use(Game *g, Ability a, Vec p) {
  if (!ability_ready(g, a)) return false;
  const AbilitySpec *s = &ABILITY_SPECS[a];
  g->money -= ability_cost(g, a);
  g->ab_cooldown_ms[a] = ability_cooldown(g, a);
  switch (a) {
    case AB_AIR:   air_strike(g, p); break;
    case AB_EW:    g->ew_point = p; g->ew_ms = (int16_t)s->duration_ms; break;
    default:       g->flare_point = p; g->flare_ms = (int16_t)s->duration_ms; break;   /* AB_FLARE */
  }
  return true;
}

static int16_t count_down(int16_t v, uint16_t dt_ms) { return v > (int16_t)dt_ms ? (int16_t)(v - dt_ms) : 0; }

void sim_tick(Game *g, uint16_t dt_ms) {
  if (g->phase != PHASE_NIGHT) return;
  g->night_ms += dt_ms;
  for (int a = 0; a < AB_COUNT; a++)
    g->ab_cooldown_ms[a] = g->ab_cooldown_ms[a] > dt_ms ? (uint16_t)(g->ab_cooldown_ms[a] - dt_ms) : 0;
  g->ew_ms = count_down(g->ew_ms, dt_ms);
  g->flare_ms = count_down(g->flare_ms, dt_ms);
  g->combo_ms = count_down(g->combo_ms, dt_ms);
  if (g->combo_ms == 0) g->combo = 0;
  while (g->wave_next < g->wave_len && g->wave[g->wave_next].time_ms <= g->night_ms) {
    if (!drone_spawn(g, &g->wave[g->wave_next])) break;
    g->wave_next++;
  }
  drones_update(g, dt_ms);
  combat_update(g, dt_ms);
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
  g->score += SCORE_PER_NIGHT;
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
