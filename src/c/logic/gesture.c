#include "gesture.h"

static int16_t iabs(int16_t v) { return v < 0 ? -v : v; }

Gesture gesture_feed(GestureTracker *t, TouchKind k, int16_t x, int16_t y, uint32_t now_ms) {
  Gesture g = { GES_NONE, x, y, 0, 0 };
  if (k == TOUCH_DOWN) {
    t->down = true; t->panning = false;
    t->x0 = t->lx = x; t->y0 = t->ly = y; t->t0 = now_ms;
    return g;
  }
  if (!t->down) return g;
  if (k == TOUCH_MOVE) {
    if (!t->panning && (iabs(x - t->x0) > TAP_SLOP || iabs(y - t->y0) > TAP_SLOP)) {
      t->panning = true;
      g.kind = GES_PAN_START;
    } else if (t->panning) {
      g.kind = GES_PAN_MOVE;
    }
    if (g.kind != GES_NONE) { g.dx = x - t->lx; g.dy = y - t->ly; }
    t->lx = x; t->ly = y;
    return g;
  }
  /* TOUCH_UP */
  if (t->panning) g.kind = GES_PAN_END;
  else if (now_ms - t->t0 <= TAP_MAX_MS && iabs(x - t->x0) <= TAP_SLOP
           && iabs(y - t->y0) <= TAP_SLOP) g.kind = GES_TAP;
  t->down = false; t->panning = false;
  return g;
}

uint32_t touch_clock_ms(TouchClock *c, uint32_t s, uint16_t ms) {
  if (!c->started) {
    c->started = true; c->s = s; c->ms = ms; c->now = 0;
    return 0;
  }
  uint32_t d = (uint32_t)(ms + 1000u - c->ms) % 1000u;
  int32_t ds = (int32_t)(s - c->s);
  if (ds >= 3) d += (uint32_t)(ds - 2) * 1000u;
  c->s = s; c->ms = ms; c->now += d;
  return c->now;
}
