#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct { int16_t x, y; uint8_t zoom; } Camera;

void camera_clamp(Camera *c, int16_t screen_w, int16_t screen_h);
void camera_pan(Camera *c, int16_t dx_px, int16_t dy_px, int16_t screen_w, int16_t screen_h);
void camera_to_world(const Camera *c, int16_t sx, int16_t sy, int16_t *wx, int16_t *wy);
void camera_to_screen(const Camera *c, int16_t wx, int16_t wy, int16_t *sx, int16_t *sy);
bool world_to_cell(int16_t wx, int16_t wy, int *col, int *row);
