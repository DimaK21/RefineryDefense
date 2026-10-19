#include <stddef.h>
#include "config.h"

const TowerSpec TOWER_SPECS[TW_COUNT] = {
  /*            name         cost range period dmg ammo hp price */
  [TW_MG]     = {"MG Nest",    30, 22,  250,  1, 0, 3, 0},
  [TW_GEPARD] = {"Gepard",     70, 34,  500,  2, 0, 5, 0},
  [TW_TOR]    = {"Tor",        90, 44, 2000,  6, 6, 3, 5},
  [TW_PATRIOT]= {"Patriot",   250, 80, 4000, 20, 3, 6, 30},
  [TW_JAMMER] = {"EW Jammer", 100, 36,    0,  0, 0, 4, 0},
  [TW_RADAR]  = {"Radar",      80, 70,    0,  0, 0, 3, 0},
};

const DroneSpec DRONE_SPECS[DR_COUNT] = {
  /*                  name          spd hp dmg tdmg bounty night threat */
  [DR_SWITCHBLADE] = {"Switchblade", 14, 2, 10, 1,  3, 1, 2},
  [DR_SHAHED136]   = {"Shahed-136",   8, 6, 35, 0,  8, 2, 4},
  [DR_GERBERA]     = {"Gerbera",      9, 2,  0, 0,  1, 3, 1},
  /* Harpy damage (to objects) applies only when no towers remain */
  [DR_HARPY]       = {"Harpy",       16, 3, 10, 4, 10, 4, 6},
  [DR_SHAHED238]   = {"Shahed-238",  22, 26, 35, 0, 15, 6, 7},
};

const AbilitySpec ABILITY_SPECS[AB_COUNT] = {
  /*              name         cost cooldown radius duration */
  [AB_AIR]   = {"Air Strike",  60, 25000, 25, 1500},
  [AB_EW]    = {"EW Pulse",    25, 12000, 40, 5000},
  [AB_FLARE] = {"Flare",       15,  8000, 45, 6000},
};

/* Normal $500: $400 bought radar + 4 shooters with $0 left and early leaks snowballed. Hard keeps $500:
 * less start money mostly added early losses decided by the seed, bigger raids raise the pressure evenly
 * (test_balance.c). */
const DifficultySpec DIFFICULTY_SPECS[DIFF_COUNT] = {
  /*                name     money wave% */
  [DIFF_EASY]   = {"Easy",    600,  70},
  [DIFF_NORMAL] = {"Normal",  500, 100},
  [DIFF_HARD]   = {"Hard",    500, 160},
};

const ObjectSpec OBJECT_SPECS[OBJ_COUNT] = {
  /*                       name                income cost col row role */
  [OBJ_DISTILLATION] = {"Distillation Column", 30, 180,  7,  8, NULL},
  [OBJ_TANK_A]       = {"Tank Farm A",         20, 120,  3, 12, "Combo x2"},
  [OBJ_TANK_B]       = {"Tank Farm B",         20, 120, 11,  4, "Missiles -50%"},
  [OBJ_TERMINAL]     = {"Loading Terminal",    25, 150, 11, 13, "Towers -15%"},
  [OBJ_CRACKER]      = {"Cracking Unit",       40, 250,  3,  4, "Abilities -30%"},
};
