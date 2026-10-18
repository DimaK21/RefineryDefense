#pragma once
#include <pebble.h>

/* Vibration and sound for the events returned by sim_take_events(). Call once per night tick. */
void feedback_events(uint32_t events);
/* Short buzz (object hit, not enough money); silent when vibration is off in Settings. */
void feedback_short(void);
/* One sample tone, played when Sound is switched on; silent when muted or off. */
void feedback_sound_preview(void);
