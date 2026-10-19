#pragma once
#include <stdint.h>
#include <stdbool.h>

#define WORLD_W 200
#define WORLD_H 228
#define CELL 12
#define GRID_COLS 16
#define GRID_ROWS 19
#define MAX_TOWERS 40
#define MAX_DRONES 64
#define MAX_MISSILES 24
#define MAX_EFFECTS 48
#define MAX_WAVE 200
#define OBJ_COUNT 5

typedef enum { TW_MG, TW_GEPARD, TW_TOR, TW_PATRIOT, TW_JAMMER, TW_RADAR, TW_COUNT } TowerType;
typedef enum { DR_SWITCHBLADE, DR_SHAHED136, DR_GERBERA, DR_HARPY, DR_SHAHED238, DR_COUNT } DroneType;
typedef enum { OBJ_DISTILLATION, OBJ_TANK_A, OBJ_TANK_B, OBJ_TERMINAL, OBJ_CRACKER } ObjectId;
typedef enum { OBJ_NOT_BUILT, OBJ_ALIVE, OBJ_DESTROYED } ObjectStatus;
typedef enum { AB_AIR, AB_EW, AB_FLARE, AB_COUNT } Ability;
typedef enum { DIFF_EASY, DIFF_NORMAL, DIFF_HARD, DIFF_COUNT } Difficulty;
typedef enum { PHASE_DAY, PHASE_NIGHT, PHASE_REPORT, PHASE_GAME_OVER } Phase;
typedef enum { TGT_OBJECT, TGT_TOWER } TargetKind;
typedef enum { FX_EXPLOSION, FX_TRACER, FX_AIRSTRIKE, FX_IMPACT, FX_BOUNTY, FX_SPAWN } EffectKind;

typedef struct { int32_t x, y; } Vec;                          // Q8 world units
typedef struct { uint8_t active, type, col, row, hp, ammo; int16_t cooldown_ms; } Tower;
typedef struct { uint8_t active, type, hp, target_kind, target_idx, wp_idx, amp;
                 uint16_t uid, phase; Vec track, pos; Vec wps[3]; } Drone;
typedef struct { uint8_t active, tower_type, drone_idx; uint16_t drone_uid; Vec pos; } Missile;
typedef struct { uint8_t active, kind, value; int16_t ttl_ms; Vec a, b; } Effect;   // value: FX_BOUNTY $
typedef struct { uint8_t status, hp; } RefObject;
typedef struct { uint16_t time_ms; uint8_t type, side; } WaveEntry;   // side: 0 top,1 right,2 bottom,3 left
typedef struct { uint16_t kills, best_combo; int32_t bounty, income; uint8_t objects_hit, towers_lost; } NightStats;

typedef struct {
  Phase phase; uint8_t difficulty; uint16_t night; int32_t money; uint32_t score, seed, rng;
  RefObject objects[OBJ_COUNT];
  Tower towers[MAX_TOWERS]; Drone drones[MAX_DRONES];
  Missile missiles[MAX_MISSILES]; Effect effects[MAX_EFFECTS];
  WaveEntry wave[MAX_WAVE]; uint16_t wave_len, wave_next, next_uid; uint32_t night_ms;
  uint16_t ab_cooldown_ms[AB_COUNT];                                // 0 = ready
  Vec ew_point, flare_point; int16_t ew_ms, flare_ms;               // active zones: ms left, 0 = off
  uint16_t combo; int16_t combo_ms;                                 // kill streak, ms left in its window
  NightStats stats; uint32_t events;
} Game;
