#include <string.h>
#include "drones.h"
#include "config.h"
#include "fx.h"
#include "rng.h"

Vec tower_center(const Tower *t) {
  Vec v = {(t->col * CELL + CELL / 2) * FX, (t->row * CELL + CELL / 2) * FX};
  return v;
}

Vec object_center(ObjectId o) {
  Vec v = {(OBJECT_SPECS[o].col * CELL + OBJECT_CELLS * CELL / 2) * FX,
           (OBJECT_SPECS[o].row * CELL + OBJECT_CELLS * CELL / 2) * FX};
  return v;
}

Effect *effect_add(Game *g, EffectKind k, Vec a, Vec b, int16_t ttl_ms) {
  for (int i = 0; i < MAX_EFFECTS; i++) {
    if (!g->effects[i].active) {
      Effect *e = &g->effects[i];
      e->active = 1; e->kind = (uint8_t)k; e->value = 0; e->ttl_ms = ttl_ms; e->a = a; e->b = b;
      return e;
    }
  }
  return NULL;
}

bool drone_jammed(const Game *g, int d) {
  const Drone *dr = &g->drones[d];
  for (int i = 0; i < MAX_TOWERS; i++) {
    const Tower *t = &g->towers[i];
    if (!t->active || t->type != TW_JAMMER) continue;
    Vec c = tower_center(t);
    if (fx_within(dr->track.x - c.x, dr->track.y - c.y, TOWER_SPECS[TW_JAMMER].range * FX))
      return true;
  }
  return false;
}

static int nearest_tower(const Game *g, Vec from, int only_type) {
  int best = -1; int32_t bd = 0;
  for (int i = 0; i < MAX_TOWERS; i++) {
    const Tower *t = &g->towers[i];
    if (!t->active || (only_type >= 0 && t->type != only_type)) continue;
    Vec c = tower_center(t);
    int32_t dist = fx_dist(c.x - from.x, c.y - from.y);
    if (best < 0 || dist < bd) { best = i; bd = dist; }
  }
  return best;
}

static int random_object(Game *g) {
  int alive[OBJ_COUNT], n = 0;
  for (int o = 0; o < OBJ_COUNT; o++)
    if (g->objects[o].status == OBJ_ALIVE) alive[n++] = o;
  if (n == 0) return -1;
  return alive[rng_range(&g->rng, 0, n - 1)];
}

/* Picks a target for drone type `type` seen from `from`; false if nothing is left to attack. */
static bool choose_target(Game *g, int type, Vec from, uint8_t *kind, uint8_t *idx, Vec *center) {
  int tw = -1;
  if (type == DR_HARPY) {
    tw = nearest_tower(g, from, TW_RADAR);
    if (tw < 0) tw = nearest_tower(g, from, -1);
  } else if (type == DR_SWITCHBLADE) {
    if (rng_range(&g->rng, 1, 100) <= SWITCHBLADE_TOWER_PCT) tw = nearest_tower(g, from, -1);
  }
  if (tw >= 0) {
    *kind = TGT_TOWER; *idx = (uint8_t)tw; *center = tower_center(&g->towers[tw]);
    return true;
  }
  int o = random_object(g);
  if (o < 0) return false;
  *kind = TGT_OBJECT; *idx = (uint8_t)o; *center = object_center((ObjectId)o);
  return true;
}

bool drone_spawn(Game *g, const WaveEntry *e) {
  int slot = -1;
  for (int i = 0; i < MAX_DRONES; i++)
    if (!g->drones[i].active) { slot = i; break; }
  if (slot < 0) return false;

  Vec p;
  switch (e->side) {
    case 0: p.x = rng_range(&g->rng, 0, WORLD_W) * FX; p.y = 0; break;
    case 1: p.x = WORLD_W * FX; p.y = rng_range(&g->rng, 0, WORLD_H) * FX; break;
    case 2: p.x = rng_range(&g->rng, 0, WORLD_W) * FX; p.y = WORLD_H * FX; break;
    default: p.x = 0; p.y = rng_range(&g->rng, 0, WORLD_H) * FX; break;
  }

  Drone d;
  memset(&d, 0, sizeof(d));
  Vec target;
  if (!choose_target(g, e->type, p, &d.target_kind, &d.target_idx, &target))
    return true;  /* nothing to attack: wave entry consumed, no drone */

  d.active = 1;
  d.type = e->type;
  d.hp = DRONE_SPECS[e->type].hp;
  d.uid = ++g->next_uid;
  d.track = d.pos = p;

  int32_t dx = target.x - p.x, dy = target.y - p.y;
  int32_t len = fx_dist(dx, dy);
  int32_t px = 0, py = 0;
  if (len > 0) { px = -dy * FX / len; py = dx * FX / len; }
  for (int k = 0; k < 2; k++) {
    int32_t off = rng_range(&g->rng, -WAYPOINT_JITTER, WAYPOINT_JITTER);
    d.wps[k].x = p.x + dx * (k + 1) / 3 + px * off;
    d.wps[k].y = p.y + dy * (k + 1) / 3 + py * off;
  }
  d.wps[2] = target;
  d.amp = (uint8_t)rng_range(&g->rng, WOBBLE_AMP_MIN, WOBBLE_AMP_MAX);
  d.phase = (uint16_t)rng_range(&g->rng, 0, 65535);
  g->drones[slot] = d;
  return true;
}

void drone_retarget(Game *g, int d) {
  Drone *dr = &g->drones[d];
  Vec target;
  uint8_t kind, idx;
  if (!choose_target(g, dr->type, dr->track, &kind, &idx, &target)) {
    dr->active = 0;
    return;
  }
  dr->target_kind = kind;
  dr->target_idx = idx;
  dr->wps[2] = target;
}

static bool target_valid(const Game *g, const Drone *dr) {
  if (dr->target_kind == TGT_TOWER)
    return dr->target_idx < MAX_TOWERS && g->towers[dr->target_idx].active;
  return dr->target_idx < OBJ_COUNT && g->objects[dr->target_idx].status == OBJ_ALIVE;
}

static void impact(Game *g, Drone *dr) {
  const DroneSpec *s = &DRONE_SPECS[dr->type];
  dr->active = 0;
  if (dr->type == DR_GERBERA) return;
  if (dr->target_kind == TGT_OBJECT) {
    RefObject *o = &g->objects[dr->target_idx];
    o->hp = (uint8_t)(o->hp > s->damage ? o->hp - s->damage : 0);
    if (o->hp == 0) { o->status = OBJ_DESTROYED; g->events |= EV_OBJECT_DESTROYED; }
    else g->events |= EV_OBJECT_HIT;
    g->stats.objects_hit++;
  } else {
    Tower *t = &g->towers[dr->target_idx];
    t->hp = (uint8_t)(t->hp > s->tower_damage ? t->hp - s->tower_damage : 0);
    if (t->hp == 0) { t->active = 0; g->events |= EV_TOWER_LOST; g->stats.towers_lost++; }
  }
  effect_add(g, FX_IMPACT, dr->wps[2], dr->wps[2], IMPACT_MS);
  g->events |= EV_EXPLOSION;
}

void drones_update(Game *g, uint16_t dt_ms) {
  for (int i = 0; i < MAX_DRONES; i++) {
    Drone *dr = &g->drones[i];
    if (!dr->active) continue;
    if (!target_valid(g, dr)) {
      drone_retarget(g, i);
      if (!dr->active) continue;
    }

    bool jammed = drone_jammed(g, i);
    int64_t step = (int64_t)DRONE_SPECS[dr->type].speed * FX * dt_ms;
    step = jammed ? step * (100 - JAMMER_SLOW_PCT) / 100000 : step / 1000;

    Vec w = dr->wps[dr->wp_idx];
    int32_t dx = w.x - dr->track.x, dy = w.y - dr->track.y;
    int32_t dist = fx_dist(dx, dy);
    if (dist > HIT_RADIUS * FX) {
      if (step >= dist) { dr->track = w; }
      else {
        dr->track.x += (int32_t)(dx * step / dist);
        dr->track.y += (int32_t)(dy * step / dist);
      }
      dx = w.x - dr->track.x; dy = w.y - dr->track.y;
      dist = fx_dist(dx, dy);
    }

    dr->phase = (uint16_t)(dr->phase + dt_ms * WOBBLE_RATE);
    int32_t amp = dr->amp * (jammed ? 2 : 1);
    int32_t px = 0, py = 0;
    if (dist > 0) { px = -dy * FX / dist; py = dx * FX / dist; }
    int32_t s = fx_sin(dr->phase);
    dr->pos.x = dr->track.x + px * amp * s / FX;
    dr->pos.y = dr->track.y + py * amp * s / FX;

    if (dist <= HIT_RADIUS * FX) {
      if (dr->wp_idx < 2) dr->wp_idx++;
      else impact(g, dr);
    }
  }
}
