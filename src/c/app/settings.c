#include <pebble.h>
#include "settings.h"

#define KEY_SOUND 5   /* keys 1-4 belong to storage.c */
#define KEY_VIBE 6

static bool s_sound = false;
static bool s_vibe = true;

void settings_load(void) {
  s_sound = persist_exists(KEY_SOUND) ? persist_read_bool(KEY_SOUND) : false;
  s_vibe = persist_exists(KEY_VIBE) ? persist_read_bool(KEY_VIBE) : true;
}

bool settings_sound(void) { return s_sound; }
bool settings_vibe(void) { return s_vibe; }

void settings_set_sound(bool on) {
  s_sound = on;
  persist_write_bool(KEY_SOUND, on);
}

void settings_set_vibe(bool on) {
  s_vibe = on;
  persist_write_bool(KEY_VIBE, on);
}
