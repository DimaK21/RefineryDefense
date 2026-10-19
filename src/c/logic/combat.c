#include "combat.h"
#include "config.h"
#include "economy.h"
#include "fx.h"

static bool is_firing(const TowerSpec *s) { return s->period_ms > 0 && s->damage > 0; }

static bool in_range(Vec a, Vec b, int32_t r) {
  return fx_within(a.x - b.x, a.y - b.y, r);
}

bool point_detected(const Game *g, Vec p) {
  if (g->flare_ms > 0 && in_range(p, g->flare_point, ABILITY_SPECS[AB_FLARE].radius * FX)) return true;
  for (int i = 0; i < MAX_TOWERS; i++) {
    const Tower *t = &g->towers[i];
    if (!t->active || t->type != TW_RADAR) continue;
    if (in_range(p, tower_center(t), TOWER_SPECS[TW_RADAR].range * FX)) return true;
  }
  return false;
}

int32_t tower_engage_range(const Game *g, int t) {
  const Tower *tw = &g->towers[t];
  int32_t units = TOWER_SPECS[tw->type].range;
  if (!point_detected(g, tower_center(tw))) units = units * DETECT_PCT / 100;
  return units * FX;
}

void combat_engage_ranges(const Game *g, int32_t out[MAX_TOWERS]) {
  for (int i = 0; i < MAX_TOWERS; i++) {
    const Tower *t = &g->towers[i];
    out[i] = t->active && is_firing(&TOWER_SPECS[t->type]) ? tower_engage_range(g, i) : 0;
  }
}

bool drone_visible_in(const Game *g, int d, const int32_t ranges[MAX_TOWERS]) {
  Vec p = g->drones[d].pos;
  if (point_detected(g, p)) return true;
  for (int i = 0; i < MAX_TOWERS; i++) {
    if (ranges[i] > 0 && in_range(p, tower_center(&g->towers[i]), ranges[i])) return true;
  }
  return false;
}

bool drone_visible(const Game *g, int d) {
  int32_t ranges[MAX_TOWERS];
  combat_engage_ranges(g, ranges);
  return drone_visible_in(g, d, ranges);
}

bool drone_identified(const Game *g, int d) {
  return point_detected(g, g->drones[d].pos);
}

int combo_pct(uint16_t combo) {
  return combo >= COMBO_X2_AT ? 200 : combo >= COMBO_X15_AT ? 150 : 100;
}

void kill_drone(Game *g, int d) {
  Drone *dr = &g->drones[d];
  if (!object_alive(g, OBJ_ROLE_COMBO)) {   /* streaks are Tank Farm A's role */
    g->combo = 0; g->combo_ms = 0;
  } else {
    if (g->combo_ms == 0) g->combo = 0;   /* streak window over (combat without sim_tick, e.g. tests) */
    if (g->combo < UINT16_MAX) g->combo++;
    g->combo_ms = COMBO_WINDOW_MS;
    if (g->combo > g->stats.best_combo) g->stats.best_combo = g->combo;
  }
  int32_t bounty = DRONE_SPECS[dr->type].bounty * combo_pct(g->combo) / 100;
  g->money += bounty; g->score += (uint32_t)bounty;
  g->stats.kills++; g->stats.bounty += bounty;
  g->events |= EV_EXPLOSION;
  effect_add(g, FX_EXPLOSION, dr->pos, dr->pos, EFFECT_MS);
  Effect *e = effect_add(g, FX_BOUNTY, dr->pos, dr->pos, BOUNTY_MS);
  if (e) e->value = (uint8_t)bounty;
  dr->active = 0;
}

static void damage_drone(Game *g, int d, uint8_t dmg) {
  Drone *dr = &g->drones[d];
  if (dr->hp <= dmg) { dr->hp = 0; kill_drone(g, d); }
  else dr->hp = (uint8_t)(dr->hp - dmg);
}

static int pick_target(const Game *g, int ti, int32_t range) {
  const Tower *t = &g->towers[ti];
  Vec c = tower_center(t);
  bool missile = t->type == TW_TOR || t->type == TW_PATRIOT;
  int best = -1; int32_t bd = 0;
  for (int i = 0; i < MAX_DRONES; i++) {
    const Drone *d = &g->drones[i];
    if (!d->active || !in_range(d->pos, c, range)) continue;
    if (missile && d->type == DR_GERBERA && drone_identified(g, i)) continue;
    int32_t rem = fx_dist(d->wps[2].x - d->track.x, d->wps[2].y - d->track.y);
    if (best < 0 || rem < bd) { best = i; bd = rem; }
  }
  return best;
}

static int free_missile(const Game *g) {
  for (int i = 0; i < MAX_MISSILES; i++) if (!g->missiles[i].active) return i;
  return -1;
}

static void update_missiles(Game *g, uint16_t dt_ms) {
  int32_t step = MISSILE_SPEED * FX * dt_ms / 1000;
  for (int i = 0; i < MAX_MISSILES; i++) {
    Missile *m = &g->missiles[i];
    if (!m->active) continue;
    const Drone *d = &g->drones[m->drone_idx];
    if (!d->active || d->uid != m->drone_uid) { m->active = 0; continue; }
    int32_t dx = d->pos.x - m->pos.x, dy = d->pos.y - m->pos.y;
    int32_t dist = fx_dist(dx, dy);
    if (dist <= step) { m->pos = d->pos; }
    else {
      m->pos.x += (int32_t)((int64_t)dx * step / dist);
      m->pos.y += (int32_t)((int64_t)dy * step / dist);
    }
    if (fx_within(d->pos.x - m->pos.x, d->pos.y - m->pos.y, HIT_RADIUS * FX)) {
      m->active = 0;
      damage_drone(g, m->drone_idx, TOWER_SPECS[m->tower_type].damage);
    }
  }
}

void combat_update(Game *g, uint16_t dt_ms) {
  update_missiles(g, dt_ms);
  int32_t ranges[MAX_TOWERS];   /* once per tick: towers do not change during combat */
  combat_engage_ranges(g, ranges);
  for (int i = 0; i < MAX_TOWERS; i++) {
    Tower *t = &g->towers[i];
    const TowerSpec *s = &TOWER_SPECS[t->type];
    if (!t->active || !is_firing(s)) continue;
    if (t->cooldown_ms > 0) {
      t->cooldown_ms = (int16_t)(t->cooldown_ms > dt_ms ? t->cooldown_ms - dt_ms : 0);
    }
    if (t->cooldown_ms > 0) continue;
    if (s->ammo != 0 && t->ammo == 0) continue;
    int target = pick_target(g, i, ranges[i]);
    if (target < 0) continue;
    Vec c = tower_center(t);
    if (t->type == TW_TOR || t->type == TW_PATRIOT) {
      int m = free_missile(g);
      if (m < 0) continue;
      Missile *ms = &g->missiles[m];
      ms->active = 1; ms->tower_type = t->type; ms->drone_idx = (uint8_t)target;
      ms->drone_uid = g->drones[target].uid; ms->pos = c;
      t->ammo--;
      g->events |= EV_MISSILE;
    } else {
      effect_add(g, FX_TRACER, c, g->drones[target].pos, TRACER_MS);
      damage_drone(g, target, s->damage);
    }
    t->cooldown_ms = (int16_t)s->period_ms;
  }
}
