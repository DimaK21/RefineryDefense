#pragma once
#include <pebble.h>
#include "../logic/types.h"
#include "../logic/camera.h"

/* All sizes are world units scaled by c->zoom; positions go through camera_to_screen,
 * so the GContext origin must be the top-left of the map viewport. */
void render_map(GContext *ctx, const Game *g, const Camera *c, bool night);
void render_towers(GContext *ctx, const Game *g, const Camera *c, bool show_ranges);
void render_entities(GContext *ctx, const Game *g, const Camera *c);

/* Tower sprite in an arbitrary screen rect (bottom bar icons, ghost). */
void render_tower_icon(GContext *ctx, int type, GRect r);
/* Translucent-looking ghost of a tower with its range circle at a cell. */
void render_ghost(GContext *ctx, const Camera *c, int type, int col, int row);
/* Highlight frame around a world rect given in cells. */
void render_cell_frame(GContext *ctx, const Camera *c, int col, int row, int cells);
