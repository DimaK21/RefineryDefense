#pragma once
#include <pebble.h>
#include "../logic/types.h"
#include "../logic/save.h"
#include "../logic/board.h"

void storage_save(const Game *g, bool pending_night);
bool storage_load(Game *g, bool *pending_night);
bool storage_exists(void);
Difficulty storage_saved_difficulty(void);   /* of the saved run, without loading it */
void storage_clear(void);
Best storage_best(Difficulty d);
void storage_set_best(Difficulty d, Best b);
/* Leaderboard name; false (and "") when none is set or the stored one is not name_valid. */
bool storage_name(char out[NAME_LEN + 1]);
void storage_set_name(const char *name);
Best storage_sent_best(Difficulty d);   /* {0, 0} until a send succeeds */
void storage_set_sent_best(Difficulty d, Best b);
