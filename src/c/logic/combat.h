#pragma once
#include "types.h"
#include "drones.h"

/* Inside a radar circle or a burning Flare: drones there are seen and identified, towers there
 * shoot at full range. */
bool    point_detected(const Game *g, Vec p);
/* Bounty percent for a kill streak of `combo` (1 = first kill). */
int     combo_pct(uint16_t combo);
int32_t tower_engage_range(const Game *g, int t);
bool    drone_visible(const Game *g, int d);
/* Engage range of every tower (0 if inactive or not firing); compute once per tick/frame. */
void    combat_engage_ranges(const Game *g, int32_t out[MAX_TOWERS]);
bool    drone_visible_in(const Game *g, int d, const int32_t ranges[MAX_TOWERS]);
bool    drone_identified(const Game *g, int d);
void    combat_update(Game *g, uint16_t dt_ms);
void    kill_drone(Game *g, int d);
