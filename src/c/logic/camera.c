#include "camera.h"
#include "types.h"

static int16_t clamp_axis(int16_t v, int16_t world, int16_t screen, uint8_t zoom) {
  int16_t max = world - screen / zoom;
  if (max < 0) max = 0;
  return v < 0 ? 0 : (v > max ? max : v);
}

void camera_clamp(Camera *c, int16_t screen_w, int16_t screen_h) {
  c->x = clamp_axis(c->x, WORLD_W, screen_w, c->zoom);
  c->y = clamp_axis(c->y, WORLD_H, screen_h, c->zoom);
}

void camera_pan(Camera *c, int16_t dx_px, int16_t dy_px, int16_t screen_w, int16_t screen_h) {
  c->x -= dx_px / c->zoom;
  c->y -= dy_px / c->zoom;
  camera_clamp(c, screen_w, screen_h);
}

void camera_to_world(const Camera *c, int16_t sx, int16_t sy, int16_t *wx, int16_t *wy) {
  *wx = c->x + sx / c->zoom;
  *wy = c->y + sy / c->zoom;
}

void camera_to_screen(const Camera *c, int16_t wx, int16_t wy, int16_t *sx, int16_t *sy) {
  *sx = (wx - c->x) * c->zoom;
  *sy = (wy - c->y) * c->zoom;
}

bool world_to_cell(int16_t wx, int16_t wy, int *col, int *row) {
  if (wx < 0 || wy < 0 || wx >= GRID_COLS * CELL || wy >= GRID_ROWS * CELL) return false;
  *col = wx / CELL;
  *row = wy / CELL;
  return true;
}
