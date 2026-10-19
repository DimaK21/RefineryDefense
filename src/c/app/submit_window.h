#pragma once
#include "../logic/types.h"
#include "../logic/save.h"

/* Sends a new record to the online leaderboard and shows the place it got. Back, Select or a
 * tap leads to the menu. The player's name must already be set. */
void submit_window_push(Difficulty d, Best b);
void submit_window_destroy(void);
