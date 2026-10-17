#pragma once
#include "types.h"

typedef struct {
  const char *name; uint16_t cost; uint8_t range; uint16_t period_ms;
  uint8_t damage, ammo /* 0 = infinite */, hp, ammo_price;
} TowerSpec;

typedef struct {
  const char *name; uint8_t speed, hp, damage, tower_damage, bounty, first_night, threat;
} DroneSpec;

typedef struct {
  const char *name; uint8_t income; uint16_t build_cost; uint8_t col, row;  /* occupies 2x2 cells */
} ObjectSpec;

extern const TowerSpec TOWER_SPECS[TW_COUNT];
extern const DroneSpec DRONE_SPECS[DR_COUNT];
extern const ObjectSpec OBJECT_SPECS[OBJ_COUNT];

#define START_MONEY 150
#define SELL_REFUND_PCT 50
#define TOWER_REPAIR_PCT 30
#define REBUILD_PCT 150
#define DETECT_PCT 60
#define JAMMER_SLOW_PCT 40
#define AIR_COST 60
#define AIR_DELAY_MS 1500
#define AIR_RADIUS 25
#define MISSILE_SPEED 60
#define SPAWN_WINDOW_MS 40000
#define SWITCHBLADE_TOWER_PCT 20
#define HIT_RADIUS 4
#define WAVE_BASE 20
#define WAVE_PER_NIGHT 12
#define WAVE_EXTRA_OBJ_PCT 10
#define EFFECT_MS 400
#define TRACER_MS 120
#define SWARM_INTERVAL_MS 300
#define SWARM_MIN 3
#define SWARM_MAX 5
#define OBJECT_CELLS 2
#define OBJECT_MAX_HP 100
#define WOBBLE_RATE 44
#define WAYPOINT_JITTER 30
#define WOBBLE_AMP_MIN 3
#define WOBBLE_AMP_MAX 8
