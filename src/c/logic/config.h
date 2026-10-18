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
  const char *name; uint16_t cost, cooldown_ms; uint8_t radius;
  uint16_t duration_ms;   /* Air Strike: delay before it lands; EW Pulse, Flare: how long the zone lasts */
} AbilitySpec;

typedef struct {
  const char *name; uint8_t income; uint16_t build_cost; uint8_t col, row;  /* occupies 2x2 cells */
} ObjectSpec;

extern const TowerSpec TOWER_SPECS[TW_COUNT];
extern const DroneSpec DRONE_SPECS[DR_COUNT];
extern const ObjectSpec OBJECT_SPECS[OBJ_COUNT];
extern const AbilitySpec ABILITY_SPECS[AB_COUNT];

#define START_MONEY 400
#define SELL_REFUND_PCT 50
#define TOWER_REPAIR_PCT 30
#define REBUILD_PCT 150
#define DETECT_PCT 60
#define JAMMER_SLOW_PCT 40
#define EW_SLOW_PCT 60          /* EW Pulse; does not stack with a Jammer, the stronger slow wins */
#define COMBO_WINDOW_MS 2000    /* a kill this soon after the previous one extends the streak */
#define COMBO_X15_AT 3          /* streak length for x1.5 bounty */
#define COMBO_X2_AT 6           /* streak length for x2 bounty */
#define SCORE_PER_NIGHT 100
#define MISSILE_SPEED 60
#define SPAWN_WINDOW_MS 40000
#define SWITCHBLADE_TOWER_PCT 20
#define HIT_RADIUS 4
#define WAVE_BASE 10
#define WAVE_PER_NIGHT 0
#define WAVE_NIGHT_SQ 2   /* + night^2 * WAVE_NIGHT_SQ threat points */
#define WAVE_HEAVY_PCT 800   /* min drone threat, % of (remaining budget / free wave slots) */
#define WAVE_EXTRA_OBJ_PCT 10
#define EFFECT_MS 400
#define IMPACT_MS 600   /* drone hitting the plant or a tower: bigger, longer blast */
#define SPAWN_WARN_MS 1200   /* edge blip where a drone has just entered the map */
#define BOUNTY_MS 700   /* floating "+$N" over a downed drone */
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
