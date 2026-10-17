#pragma once
#include <pebble.h>

/* Vibration and sound for the events returned by sim_take_events(). Call once per night tick. */
void feedback_events(uint32_t events);
