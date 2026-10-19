#pragma once
#include "../logic/types.h"

/* Online leaderboard per difficulty: top rows and the player's own place. Select (or a tap)
 * switches difficulty, Up/Down scroll, held Select sends a best the board has not seen yet. */
void leaders_window_push(Difficulty d);
void leaders_window_destroy(void);
