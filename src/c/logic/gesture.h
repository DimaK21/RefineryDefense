#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum { TOUCH_DOWN, TOUCH_UP, TOUCH_MOVE } TouchKind;
typedef enum { GES_NONE, GES_TAP, GES_PAN_START, GES_PAN_MOVE, GES_PAN_END } GestureKind;
typedef struct { GestureKind kind; int16_t x, y, dx, dy; } Gesture;
typedef struct { bool down, panning; int16_t x0, y0, lx, ly; uint32_t t0; } GestureTracker;

#define TAP_SLOP 6
#define TAP_MAX_MS 500

Gesture gesture_feed(GestureTracker *t, TouchKind k, int16_t x, int16_t y, uint32_t now_ms);

/* Monotonic ms for gesture_feed from time_ms() samples. The seconds and ms fields of time_ms()
 * are not in phase (observed on the emulator: seconds stepped back or ran ahead by one), so
 * steps come from the ms field alone; seconds add whole extra seconds only for gaps of 3 s+. */
typedef struct { bool started; uint32_t s, now; uint16_t ms; } TouchClock;
uint32_t touch_clock_ms(TouchClock *c, uint32_t s, uint16_t ms);
