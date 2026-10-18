#pragma once
#include <pebble.h>
#include "../logic/types.h"
#include "../logic/save.h"

void storage_save(const Game *g, bool pending_night);
bool storage_load(Game *g, bool *pending_night);
bool storage_exists(void);
Difficulty storage_saved_difficulty(void);   /* of the saved run, without loading it */
void storage_clear(void);
Best storage_best(Difficulty d);
void storage_set_best(Difficulty d, Best b);
