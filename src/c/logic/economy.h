#pragma once
#include "types.h"

void game_new(Game *g, uint32_t seed, Difficulty d);
int alive_objects(const Game *g);

typedef enum { ACT_OK, ACT_NO_MONEY, ACT_BLOCKED, ACT_INVALID } ActResult;

int  tower_at(const Game *g, int col, int row);
int  object_at(const Game *g, int col, int row);
bool cell_free(const Game *g, int col, int row);
ActResult tower_buy(Game *g, TowerType t, int col, int row);
ActResult tower_move(Game *g, int idx, int col, int row);
int32_t   tower_sell_value(const Game *g, int idx);
ActResult tower_sell(Game *g, int idx);
int32_t   tower_repair_cost(const Game *g, int idx);
ActResult tower_repair(Game *g, int idx);
int32_t   tower_reload_cost(const Game *g, int idx);
ActResult tower_reload(Game *g, int idx);
int32_t   object_action_cost(const Game *g, ObjectId o);
ActResult object_action(Game *g, ObjectId o);
int32_t   morning_income(const Game *g);
int       plant_health_pct(const Game *g);   /* alive HP / built capacity, 0 if none built */
