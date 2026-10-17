#pragma once
#include "types.h"
#include "drones.h"

bool    point_in_radar(const Game *g, Vec p);
int32_t tower_engage_range(const Game *g, int t);
bool    drone_visible(const Game *g, int d);
bool    drone_identified(const Game *g, int d);
void    combat_update(Game *g, uint16_t dt_ms);
void    kill_drone(Game *g, int d);
