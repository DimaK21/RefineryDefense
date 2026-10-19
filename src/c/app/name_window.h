#pragma once
#include <stdbool.h>

/* Leaderboard name entry: NAME_LEN slots, Up/Down (held: fast) change the letter, Select moves on,
 * held Select saves, Back steps back or cancels. done(saved) runs after the window is gone. */
typedef void (*NameDone)(bool saved);
void name_window_push(NameDone done);
void name_window_destroy(void);
