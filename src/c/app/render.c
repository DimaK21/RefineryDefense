#include "render.h"
#include "ui_colors.h"
#include "../logic/config.h"
#include "../logic/fx.h"
#include "../logic/combat.h"

#define OBJ_UNITS (OBJECT_CELLS * CELL)
#define HP_BAR_UNITS 2
#define DASH_PX 3
#define EXPLOSION_UNITS 5
#define MISSILE_UNITS 1
#define TRAIL_UNITS 6
#define AIRSTRIKE_MIN_UNITS 3

static const char OBJECT_LETTERS[OBJ_COUNT] = {'D', 'A', 'B', 'L', 'C'};

static GPoint prv_pt(const Camera *c, int wx, int wy) {
  int16_t sx, sy;
  camera_to_screen(c, (int16_t)wx, (int16_t)wy, &sx, &sy);
  return GPoint(sx, sy);
}

static GPoint prv_vec(const Camera *c, Vec v) { return prv_pt(c, v.x / FX, v.y / FX); }

static GRect prv_rect(const Camera *c, int wx, int wy, int ww, int wh) {
  GPoint p = prv_pt(c, wx, wy);
  return GRect(p.x, p.y, ww * c->zoom, wh * c->zoom);
}

static void prv_letter(GContext *ctx, char ch, GRect r, GColor color) {
  char buf[2] = {ch, 0};
  GFont font = fonts_get_system_font(r.size.h >= 20 ? FONT_KEY_GOTHIC_18_BOLD
                                     : r.size.h >= 14 ? FONT_KEY_GOTHIC_14_BOLD
                                     : FONT_KEY_GOTHIC_09);
  int fh = r.size.h >= 20 ? 18 : r.size.h >= 14 ? 14 : 9;
  /* Gothic glyphs sit low in their box: lift by a third of the font height. */
  GRect box = GRect(r.origin.x, r.origin.y + (r.size.h - fh) / 2 - fh / 3, r.size.w, fh + 4);
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, buf, font, box, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void prv_dashed_rect(GContext *ctx, GRect r) {
  int x1 = r.origin.x + r.size.w - 1, y1 = r.origin.y + r.size.h - 1;
  for (int x = r.origin.x; x <= x1; x += 2 * DASH_PX) {
    int e = x + DASH_PX - 1 > x1 ? x1 : x + DASH_PX - 1;
    graphics_draw_line(ctx, GPoint(x, r.origin.y), GPoint(e, r.origin.y));
    graphics_draw_line(ctx, GPoint(x, y1), GPoint(e, y1));
  }
  for (int y = r.origin.y; y <= y1; y += 2 * DASH_PX) {
    int e = y + DASH_PX - 1 > y1 ? y1 : y + DASH_PX - 1;
    graphics_draw_line(ctx, GPoint(r.origin.x, y), GPoint(r.origin.x, e));
    graphics_draw_line(ctx, GPoint(x1, y), GPoint(x1, e));
  }
}

static void prv_hp_bar(GContext *ctx, GRect r, int hp, int max, int bar_h) {
  if (bar_h < 1) bar_h = 1;
  GRect bar = GRect(r.origin.x + 1, r.origin.y + r.size.h - bar_h - 1, r.size.w - 2, bar_h);
  graphics_context_set_fill_color(ctx, COL_HP_EMPTY);
  graphics_fill_rect(ctx, bar, 0, GCornerNone);
  bar.size.w = bar.size.w * hp / max;
  graphics_context_set_fill_color(ctx, COL_HP_FULL);
  graphics_fill_rect(ctx, bar, 0, GCornerNone);
}

static void prv_object(GContext *ctx, const Game *g, const Camera *c, int o) {
  const ObjectSpec *s = &OBJECT_SPECS[o];
  GRect r = prv_rect(c, s->col * CELL, s->row * CELL, OBJ_UNITS, OBJ_UNITS);
  GRect in = grect_inset(r, GEdgeInsets(c->zoom));
  switch (g->objects[o].status) {
    case OBJ_ALIVE:
      graphics_context_set_fill_color(ctx, object_color(o));
      graphics_fill_rect(ctx, in, 2 * c->zoom, GCornersAll);
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_draw_round_rect(ctx, in, 2 * c->zoom);
      prv_letter(ctx, OBJECT_LETTERS[o], in, GColorBlack);
      prv_hp_bar(ctx, in, g->objects[o].hp, OBJECT_MAX_HP, HP_BAR_UNITS * c->zoom);
      break;
    case OBJ_DESTROYED:
      graphics_context_set_fill_color(ctx, COL_DESTROYED);
      graphics_fill_rect(ctx, in, 0, GCornerNone);
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_context_set_stroke_width(ctx, c->zoom > 1 ? 3 : 1);
      graphics_draw_line(ctx, in.origin, GPoint(in.origin.x + in.size.w - 1, in.origin.y + in.size.h - 1));
      graphics_draw_line(ctx, GPoint(in.origin.x + in.size.w - 1, in.origin.y),
                         GPoint(in.origin.x, in.origin.y + in.size.h - 1));
      graphics_context_set_stroke_width(ctx, 1);
      break;
    default:   /* OBJ_NOT_BUILT */
      graphics_context_set_stroke_color(ctx, COL_NOT_BUILT);
      prv_dashed_rect(ctx, in);
      prv_letter(ctx, OBJECT_LETTERS[o], in, COL_NOT_BUILT);
      break;
  }
}

void render_map(GContext *ctx, const Game *g, const Camera *c, bool night) {
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_fill_color(ctx, night ? COL_NIGHT_BG : COL_DAY_BG);
  graphics_fill_rect(ctx, prv_rect(c, 0, 0, WORLD_W, WORLD_H), 0, GCornerNone);
  if (!night) {
    graphics_context_set_stroke_color(ctx, COL_GRID);
    GPoint a = prv_pt(c, 0, 0), b = prv_pt(c, GRID_COLS * CELL, GRID_ROWS * CELL);
    for (int col = 0; col <= GRID_COLS; col++) {
      int x = prv_pt(c, col * CELL, 0).x;
      graphics_draw_line(ctx, GPoint(x, a.y), GPoint(x, b.y));
    }
    for (int row = 0; row <= GRID_ROWS; row++) {
      int y = prv_pt(c, 0, row * CELL).y;
      graphics_draw_line(ctx, GPoint(a.x, y), GPoint(b.x, y));
    }
  }
  for (int o = 0; o < OBJ_COUNT; o++) prv_object(ctx, g, c, o);
}

void render_tower_icon(GContext *ctx, int type, GRect r) {
  GColor col = tower_color(type);
  int m = r.size.w < r.size.h ? r.size.w : r.size.h;
  GPoint ctr = GPoint(r.origin.x + r.size.w / 2, r.origin.y + r.size.h / 2);
  graphics_context_set_fill_color(ctx, col);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  switch (type) {
    case TW_MG:
    case TW_GEPARD:   /* guns: square / rounded square */
      graphics_fill_rect(ctx, r, type == TW_GEPARD ? m / 4 : 0, GCornersAll);
      if (type == TW_GEPARD) graphics_draw_round_rect(ctx, r, m / 4);
      else graphics_draw_rect(ctx, r);
      break;
    case TW_TOR:
    case TW_PATRIOT:  /* missiles: circle */
      graphics_fill_circle(ctx, ctr, m / 2);
      graphics_draw_circle(ctx, ctr, m / 2);
      break;
    case TW_JAMMER: { /* diamond */
      GPoint pts[4] = {GPoint(ctr.x, r.origin.y), GPoint(r.origin.x + r.size.w - 1, ctr.y),
                       GPoint(ctr.x, r.origin.y + r.size.h - 1), GPoint(r.origin.x, ctr.y)};
      GPath path = {4, pts, 0, GPointZero};
      gpath_draw_filled(ctx, &path);
      gpath_draw_outline(ctx, &path);
      break;
    }
    default:          /* radar: dish ring */
      graphics_fill_circle(ctx, ctr, m / 2);
      graphics_draw_circle(ctx, ctr, m / 2);
      graphics_draw_circle(ctx, ctr, m / 4);
      break;
  }
  prv_letter(ctx, tower_letter(type), r, type == TW_PATRIOT || type == TW_JAMMER ? GColorWhite : GColorBlack);
}

static GRect prv_cell_rect(const Camera *c, int col, int row) {
  return grect_inset(prv_rect(c, col * CELL, row * CELL, CELL, CELL), GEdgeInsets(c->zoom));
}

static void prv_range(GContext *ctx, const Camera *c, int type, int col, int row, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_draw_circle(ctx, prv_pt(c, col * CELL + CELL / 2, row * CELL + CELL / 2),
                       TOWER_SPECS[type].range * c->zoom);
}

void render_towers(GContext *ctx, const Game *g, const Camera *c, bool show_ranges) {
  if (show_ranges)
    for (int i = 0; i < MAX_TOWERS; i++) {
      const Tower *t = &g->towers[i];
      if (t->active) prv_range(ctx, c, t->type, t->col, t->row, COL_RANGE);
    }
  for (int i = 0; i < MAX_TOWERS; i++) {
    const Tower *t = &g->towers[i];
    if (!t->active) continue;
    GRect r = prv_cell_rect(c, t->col, t->row);
    render_tower_icon(ctx, t->type, r);
    int max = TOWER_SPECS[t->type].hp;
    if (t->hp < max) prv_hp_bar(ctx, r, t->hp, max, c->zoom);
  }
}

void render_ghost(GContext *ctx, const Camera *c, int type, int col, int row) {
  prv_range(ctx, c, type, col, row, COL_GHOST);
  GRect r = prv_cell_rect(c, col, row);
  graphics_context_set_stroke_color(ctx, COL_GHOST);
  prv_dashed_rect(ctx, grect_inset(r, GEdgeInsets(-c->zoom)));
  render_tower_icon(ctx, type, grect_inset(r, GEdgeInsets(c->zoom)));
}

void render_cell_frame(GContext *ctx, const Camera *c, int col, int row, int cells) {
  graphics_context_set_stroke_color(ctx, COL_SELECT);
  graphics_context_set_stroke_width(ctx, 1);
  GRect r = prv_rect(c, col * CELL, row * CELL, cells * CELL, cells * CELL);
  graphics_draw_rect(ctx, r);
  graphics_draw_rect(ctx, grect_inset(r, GEdgeInsets(1)));
}

static void prv_drone(GContext *ctx, const Game *g, const Camera *c, int d) {
  const Drone *dr = &g->drones[d];
  int type = dr->type;
  if (type == DR_GERBERA && !drone_identified(g, d)) type = DR_SHAHED136;
  GPoint p = prv_vec(c, dr->pos);
  int r = (type == DR_SWITCHBLADE ? 1 : 2) * c->zoom;
  if (c->zoom == 1) r++;   /* night x1: a 1 px radius is a speck */
  if (type == DR_GERBERA) {
    graphics_context_set_stroke_color(ctx, COL_DECOY);
    graphics_draw_circle(ctx, p, r);
    return;
  }
  /* Trail: a short segment behind the drone, opposite to its heading on the track. */
  Vec w = dr->wps[dr->wp_idx];
  int32_t dx = w.x - dr->track.x, dy = w.y - dr->track.y;
  int32_t len = fx_dist(dx, dy);
  if (len > 0) {
    GPoint t = GPoint(p.x - (int)(dx * TRAIL_UNITS * c->zoom / len),
                      p.y - (int)(dy * TRAIL_UNITS * c->zoom / len));
    graphics_context_set_stroke_color(ctx, COL_DRONE_TRAIL);
    graphics_draw_line(ctx, p, t);
  }
  graphics_context_set_fill_color(ctx, COL_DRONE);
  graphics_fill_circle(ctx, p, r);
  if (type == DR_SHAHED136 || type == DR_SHAHED238) {   /* delta wing hint */
    graphics_context_set_stroke_color(ctx, COL_DRONE);
    graphics_draw_line(ctx, GPoint(p.x - 2 * r, p.y + r), GPoint(p.x + 2 * r, p.y + r));
  }
}

void render_entities(GContext *ctx, const Game *g, const Camera *c) {
  static int32_t s_ranges[MAX_TOWERS];   /* once per frame, not per drone */
  combat_engage_ranges(g, s_ranges);
  for (int d = 0; d < MAX_DRONES; d++)
    if (g->drones[d].active && drone_visible_in(g, d, s_ranges)) prv_drone(ctx, g, c, d);
  graphics_context_set_fill_color(ctx, COL_MISSILE);
  for (int m = 0; m < MAX_MISSILES; m++)
    if (g->missiles[m].active)
      graphics_fill_circle(ctx, prv_vec(c, g->missiles[m].pos), MISSILE_UNITS * c->zoom);
  for (int e = 0; e < MAX_EFFECTS; e++) {
    const Effect *ef = &g->effects[e];
    if (!ef->active) continue;
    switch (ef->kind) {
      case FX_TRACER:
        graphics_context_set_stroke_color(ctx, COL_MISSILE);
        graphics_draw_line(ctx, prv_vec(c, ef->a), prv_vec(c, ef->b));
        break;
      case FX_AIRSTRIKE: {   /* ring expanding to AIR_RADIUS over the effect's lifetime */
        int ttl = ef->ttl_ms < 0 ? 0 : ef->ttl_ms;
        int rad = AIRSTRIKE_MIN_UNITS + (AIR_RADIUS - AIRSTRIKE_MIN_UNITS) * (EFFECT_MS - ttl) / EFFECT_MS;
        graphics_context_set_stroke_color(ctx, COL_EXPLOSION);
        graphics_context_set_stroke_width(ctx, 3);
        graphics_draw_circle(ctx, prv_vec(c, ef->a), rad * c->zoom);
        graphics_context_set_stroke_width(ctx, 1);
        break;
      }
      default:   /* FX_EXPLOSION */
        graphics_context_set_fill_color(ctx, COL_EXPLOSION);
        graphics_fill_circle(ctx, prv_vec(c, ef->a), EXPLOSION_UNITS * c->zoom);
        break;
    }
  }
}
