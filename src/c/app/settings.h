#pragma once
#include <stdbool.h>

/* Player preferences, kept apart from the game save so they survive Game Over.
 * Defaults: sound off, vibration on. */
void settings_load(void);
bool settings_sound(void);
bool settings_vibe(void);
void settings_set_sound(bool on);
void settings_set_vibe(bool on);
