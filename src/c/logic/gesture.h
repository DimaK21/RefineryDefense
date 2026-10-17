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
