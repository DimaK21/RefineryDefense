#pragma once
#include <pebble.h>
#include "../logic/types.h"
#include "../logic/save.h"

void storage_save(const Game *g, bool pending_night);
bool storage_load(Game *g, bool *pending_night);
bool storage_exists(void);
void storage_clear(void);
Best storage_best(void);
void storage_set_best(Best b);
