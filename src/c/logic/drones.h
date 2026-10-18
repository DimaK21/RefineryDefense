#pragma once
#include "types.h"
#include "waves.h"

#define EV_OBJECT_HIT 1u
#define EV_OBJECT_DESTROYED 2u
#define EV_MISSILE 4u
#define EV_EXPLOSION 8u
#define EV_TOWER_LOST 16u

bool drone_spawn(Game *g, const WaveEntry *e);   /* false only if the pool is full */
void drones_update(Game *g, uint16_t dt_ms);
bool drone_jammed(const Game *g, int d);
Vec  tower_center(const Tower *t);
Vec  object_center(ObjectId o);
void drone_retarget(Game *g, int d);
/* NULL when the effect pool is full: effects are cosmetic and may be dropped. */
Effect *effect_add(Game *g, EffectKind k, Vec a, Vec b, int16_t ttl_ms);
