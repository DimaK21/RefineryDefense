#pragma once
#include "types.h"

/* New game: the centre plus one random unit of the other four. */
void game_new(Game *g, uint32_t seed, Difficulty d);
/* Same with the second unit given; draws nothing from the RNG beyond what game_new does first. */
void game_new_start(Game *g, uint32_t seed, Difficulty d, ObjectId second);
int  alive_objects(const Game *g);
bool object_alive(const Game *g, ObjectId o);
int32_t discounted(int32_t cost, int off_pct);   /* rounded down, at least 1 for a nonzero cost */

typedef enum { ACT_OK, ACT_NO_MONEY, ACT_BLOCKED, ACT_INVALID } ActResult;

int  tower_at(const Game *g, int col, int row);
int  object_at(const Game *g, int col, int row);
bool cell_free(const Game *g, int col, int row);
int32_t   tower_price(const Game *g, TowerType t);     /* purchase price, Loading Terminal discount */
int32_t   missile_price(const Game *g, TowerType t);   /* one missile, Tank Farm B discount; 0: no missiles */
ActResult tower_buy(Game *g, TowerType t, int col, int row);
ActResult tower_move(Game *g, int idx, int col, int row);
int32_t   tower_sell_value(const Game *g, int idx);
ActResult tower_sell(Game *g, int idx);
int32_t   tower_repair_cost(const Game *g, int idx);
ActResult tower_repair(Game *g, int idx);
int32_t   tower_reload_cost(const Game *g, int idx);
ActResult tower_reload(Game *g, int idx);
int32_t   reload_all_cost(const Game *g);
/* Missiles for every Tor/Patriot; short of money, one missile at a time to the emptiest tower.
 * ACT_NO_MONEY: not even one missile affordable, nothing spent. */
ActResult reload_all(Game *g);
int32_t   object_action_cost(const Game *g, ObjectId o);
ActResult object_action(Game *g, ObjectId o);
int32_t   morning_income(const Game *g);
int       plant_health_pct(const Game *g);   /* alive HP / built capacity, 0 if none built */
