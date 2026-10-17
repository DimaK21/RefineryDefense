#include "test.h"
#include "gesture.h"
#include "camera.h"

static void test_tap(void) {
  GestureTracker t = {0};
  Gesture g = gesture_feed(&t, TOUCH_DOWN, 50, 50, 1000);
  CHECK_EQ(g.kind, GES_NONE);
  g = gesture_feed(&t, TOUCH_UP, 52, 51, 1200);
  CHECK_EQ(g.kind, GES_TAP); CHECK_EQ(g.x, 52); CHECK_EQ(g.y, 51);
  CHECK(!t.down);
}

static void test_long_hold(void) {
  GestureTracker t = {0};
  gesture_feed(&t, TOUCH_DOWN, 50, 50, 1000);
  Gesture g = gesture_feed(&t, TOUCH_UP, 50, 50, 1800);
  CHECK_EQ(g.kind, GES_NONE);
}

static void test_pan(void) {
  GestureTracker t = {0};
  gesture_feed(&t, TOUCH_DOWN, 50, 50, 1000);
  Gesture g = gesture_feed(&t, TOUCH_MOVE, 60, 50, 1050);
  CHECK_EQ(g.kind, GES_PAN_START); CHECK_EQ(g.dx, 10); CHECK_EQ(g.dy, 0);
  g = gesture_feed(&t, TOUCH_MOVE, 65, 50, 1100);
  CHECK_EQ(g.kind, GES_PAN_MOVE); CHECK_EQ(g.dx, 5); CHECK_EQ(g.dy, 0);
  g = gesture_feed(&t, TOUCH_UP, 65, 50, 1150);
  CHECK_EQ(g.kind, GES_PAN_END);
  CHECK(!t.down && !t.panning);
}

static void test_slop_and_orphans(void) {
  GestureTracker t = {0};
  Gesture g = gesture_feed(&t, TOUCH_MOVE, 10, 10, 0);
  CHECK_EQ(g.kind, GES_NONE);
  g = gesture_feed(&t, TOUCH_UP, 10, 10, 0);
  CHECK_EQ(g.kind, GES_NONE);
  gesture_feed(&t, TOUCH_DOWN, 50, 50, 0);
  g = gesture_feed(&t, TOUCH_MOVE, 55, 50, 10);   /* within slop */
  CHECK_EQ(g.kind, GES_NONE);
  g = gesture_feed(&t, TOUCH_UP, 55, 50, 100);
  CHECK_EQ(g.kind, GES_TAP);
}

static void test_camera(void) {
  Camera c = {0, 0, 2};
  camera_pan(&c, -400, -400, 200, 228);
  CHECK_EQ(c.x, 100); CHECK_EQ(c.y, 114);
  camera_pan(&c, 1000, 1000, 200, 228);
  CHECK_EQ(c.x, 0); CHECK_EQ(c.y, 0);
  c = (Camera){50, 60, 2};
  int16_t wx, wy, sx, sy;
  camera_to_world(&c, 24, 24, &wx, &wy);
  CHECK_EQ(wx, 62); CHECK_EQ(wy, 72);
  camera_to_screen(&c, 62, 72, &sx, &sy);
  CHECK_EQ(sx, 24); CHECK_EQ(sy, 24);
  Camera n = {0, 0, 1};
  camera_clamp(&n, 200, 228);
  CHECK_EQ(n.x, 0); CHECK_EQ(n.y, 0);
}

static void test_cell(void) {
  int col, row;
  CHECK(world_to_cell(13, 25, &col, &row)); CHECK_EQ(col, 1); CHECK_EQ(row, 2);
  CHECK(!world_to_cell(195, 10, &col, &row));
  CHECK(!world_to_cell(-1, 10, &col, &row));
  CHECK(!world_to_cell(10, -1, &col, &row));
  CHECK(!world_to_cell(10, 19 * 12, &col, &row));
}

static void test_touch_clock(void) {
  TouchClock c = {0};
  CHECK_EQ(touch_clock_ms(&c, 100, 506), 0);                   /* first sample is the origin */
  CHECK_EQ(touch_clock_ms(&c, 99, 607), 101);                  /* seconds lag the ms field */
  CHECK_EQ(touch_clock_ms(&c, 100, 710), 204);
  CHECK_EQ(touch_clock_ms(&c, 101, 20), 514);                  /* ms wraps */
  CHECK_EQ(touch_clock_ms(&c, 105, 20), 2514);                 /* long gap: whole extra seconds */
}

int main(void) {
  RUN(test_touch_clock);
  RUN(test_tap); RUN(test_long_hold); RUN(test_pan);
  RUN(test_slop_and_orphans); RUN(test_camera); RUN(test_cell);
  return TEST_SUMMARY();
}
