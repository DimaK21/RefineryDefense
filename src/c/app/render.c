#include "render.h"
#include "ui_colors.h"
#include "../logic/config.h"
#include "../logic/fx.h"
#include "../logic/combat.h"
#include "../logic/waves.h"

#define OBJ_UNITS (OBJECT_CELLS * CELL)
#define HP_BAR_UNITS 2
#define DASH_PX 3
#define EXPLOSION_UNITS 5
#define IMPACT_UNITS 8
#define BOUNTY_RISE_UNITS 12
#define FIRE_FRAME_MS 120    /* flame flicker step at night (the day screen is static) */
#define SMOKE_FRAME_MS 80
#define RAID_ARROW_W 14   /* raid direction chevron: base width and depth, px */
#define RAID_ARROW_D 7
#define FIRE_HP 66           /* an object at or below this HP burns; at or below FIRE_HP / 2, twice */
#define MISSILE_UNITS 1
#define TRAIL_UNITS 6
#define MISSILE_TRAIL_UNITS 5
#define AIRSTRIKE_MIN_UNITS 3
#define SPRITE_GRID 11   /* plant sprites are drawn on an 11x11 grid over the object */

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

/* Rect on an SPRITE_GRID x SPRITE_GRID grid laid over `in`. */
static GRect prv_sp(GRect in, int x, int y, int w, int h) {
  int u = in.size.w;
  return GRect(in.origin.x + x * u / SPRITE_GRID, in.origin.y + y * u / SPRITE_GRID,
               w * u / SPRITE_GRID, h * u / SPRITE_GRID);
}

static void prv_sp_box(GContext *ctx, GRect r, int radius) {
  graphics_fill_rect(ctx, r, radius, GCornersAll);
  graphics_draw_round_rect(ctx, r, radius);
}

static void prv_sp_disc(GContext *ctx, GRect in, int x, int y, int rad) {
  int u = in.size.w;
  GPoint p = GPoint(in.origin.x + x * u / (2 * SPRITE_GRID), in.origin.y + y * u / (2 * SPRITE_GRID));
  int pr = rad * u / (2 * SPRITE_GRID);
  graphics_fill_circle(ctx, p, pr);
  graphics_draw_circle(ctx, p, pr);
}

/* Pictures of the plant units on a concrete pad; coordinates in sprite cells (discs in half cells). */
static void prv_object_sprite(GContext *ctx, GRect in, int o, int zoom) {
  graphics_context_set_fill_color(ctx, COL_PAD);
  graphics_fill_rect(ctx, in, 2 * zoom, GCornersAll);
  graphics_context_set_fill_color(ctx, object_color(o));
  graphics_context_set_stroke_color(ctx, GColorBlack);
  switch (o) {
    case OBJ_DISTILLATION:   /* two columns with a pipe bridge */
      prv_sp_box(ctx, prv_sp(in, 2, 1, 3, 9), zoom);
      prv_sp_box(ctx, prv_sp(in, 6, 3, 3, 7), zoom);
      graphics_draw_line(ctx, prv_sp(in, 5, 4, 0, 0).origin, prv_sp(in, 6, 4, 0, 0).origin);
      for (int y = 4; y < 10; y += 3) {   /* column rings */
        GRect a = prv_sp(in, 2, y, 3, 0), b = prv_sp(in, 6, y + 1, 3, 0);
        graphics_draw_line(ctx, a.origin, GPoint(a.origin.x + a.size.w - 1, a.origin.y));
        if (y + 1 < 10) graphics_draw_line(ctx, b.origin, GPoint(b.origin.x + b.size.w - 1, b.origin.y));
      }
      break;
    case OBJ_TANK_A:
    case OBJ_TANK_B:         /* four storage tanks */
      prv_sp_disc(ctx, in, 6, 6, 4);
      prv_sp_disc(ctx, in, 16, 6, 4);
      prv_sp_disc(ctx, in, 6, 15, 4);
      prv_sp_disc(ctx, in, 16, 15, 4);
      break;
    case OBJ_TERMINAL:       /* pier, crane and containers */
      prv_sp_box(ctx, prv_sp(in, 1, 7, 9, 3), 0);
      prv_sp_box(ctx, prv_sp(in, 2, 1, 2, 6), 0);
      prv_sp_box(ctx, prv_sp(in, 2, 1, 8, 1), 0);
      prv_sp_box(ctx, prv_sp(in, 5, 4, 2, 3), 0);
      prv_sp_box(ctx, prv_sp(in, 7, 4, 2, 3), 0);
      break;
    default:                 /* OBJ_CRACKER: reactor, unit block and stack */
      prv_sp_box(ctx, prv_sp(in, 1, 5, 6, 5), zoom);
      prv_sp_disc(ctx, in, 8, 7, 3);
      prv_sp_box(ctx, prv_sp(in, 8, 1, 2, 9), 0);
      break;
  }
}

/* Tongue of fire rising from a disc of radius `w` at `c`: one row per pixel, narrowing to a tip
 * `h` px above and leaning `sway` px sideways. */
static void prv_tongue(GContext *ctx, GPoint c, int w, int h, int sway, GColor col) {
  graphics_context_set_fill_color(ctx, col);
  graphics_fill_circle(ctx, c, w);
  graphics_context_set_stroke_color(ctx, col);
  for (int i = 0; i < h; i++) {
    int hw = w * (h - i) / h, x = c.x + sway * i / h;
    graphics_draw_line(ctx, GPoint(x - hw, c.y - i), GPoint(x + hw, c.y - i));
  }
}

/* Flame standing on `base`: orange tongue with a yellow core; `flick` 0..2 sways and stretches it. */
static void prv_flame(GContext *ctx, GPoint base, int s, int flick) {
  int h = 3 * s + flick * s / 2;
  int sway = (flick - 1) * s / 2;
  GPoint c = GPoint(base.x, base.y - s);
  prv_tongue(ctx, c, s, h, sway, GColorOrange);
  prv_tongue(ctx, c, s / 2 > 0 ? s / 2 : 1, h * 2 / 3, sway / 2, GColorYellow);
}

/* Three puffs rising from `from` and growing as they go; `t` drives the drift. Light gray: the
 * dark concrete pad and the night sky would swallow dark smoke. */
static void prv_smoke(GContext *ctx, GPoint from, int s, uint32_t t) {
  if (s < 2) s = 2;
  int span = 5 * s;
  graphics_context_set_fill_color(ctx, GColorLightGray);
  for (int k = 0; k < 3; k++) {
    int rise = (int)((t / SMOKE_FRAME_MS * (uint32_t)s / 2 + (uint32_t)(k * span / 3)) % (uint32_t)span);
    int r = s / 2 + rise * s / span;
    graphics_fill_circle(ctx, GPoint(from.x + (k - 1) * s / 2, from.y - rise), r > 0 ? r : 1);
  }
}

/* Damage on a standing object: smoke and one flame, a second flame when badly hurt. */
static void prv_fire(GContext *ctx, GRect in, int hp, int zoom, uint32_t t) {
  if (hp > FIRE_HP) return;
  int s = 2 + zoom;   /* flame half-width, px: readable at night x1 without swamping the day x2 sprite */
  int f = (int)(t / FIRE_FRAME_MS);
  GPoint a = prv_sp(in, 3, 8, 0, 0).origin;
  prv_smoke(ctx, GPoint(a.x, a.y - 3 * s), zoom + 1, t);
  prv_flame(ctx, a, s, f % 3);
  if (hp <= FIRE_HP / 2) prv_flame(ctx, prv_sp(in, 8, 9, 0, 0).origin, s, (f + 1) % 3);
}

static void prv_object(GContext *ctx, const Game *g, const Camera *c, int o, uint32_t t) {
  const ObjectSpec *s = &OBJECT_SPECS[o];
  GRect r = prv_rect(c, s->col * CELL, s->row * CELL, OBJ_UNITS, OBJ_UNITS);
  GRect in = grect_inset(r, GEdgeInsets(c->zoom));
  switch (g->objects[o].status) {
    case OBJ_ALIVE:
      prv_object_sprite(ctx, in, o, c->zoom);
      prv_fire(ctx, in, g->objects[o].hp, c->zoom, t);
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
      prv_smoke(ctx, prv_sp(in, 5, 3, 0, 0).origin, c->zoom + 1, t);   /* smouldering */
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
  uint32_t t = night ? g->night_ms : 0;   /* animation clock; the day screen is a still frame */
  for (int o = 0; o < OBJ_COUNT; o++) prv_object(ctx, g, c, o, t);
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

/* Filled triangle by horizontal spans (no GPath: its fill dropped small triangles on emery). */
static void prv_fill_tri(GContext *ctx, GPoint a, GPoint b, GPoint c) {
  GPoint t;
  if (b.y < a.y) { t = a; a = b; b = t; }
  if (c.y < a.y) { t = a; a = c; c = t; }
  if (c.y < b.y) { t = b; b = c; c = t; }
  if (c.y == a.y) {   /* flat: one span from the leftmost to the rightmost point */
    int lo = a.x < b.x ? a.x : b.x, hi = a.x < b.x ? b.x : a.x;
    graphics_draw_line(ctx, GPoint(lo < c.x ? lo : c.x, a.y), GPoint(hi > c.x ? hi : c.x, a.y));
    return;
  }
  for (int y = a.y; y <= c.y; y++) {
    int xl = a.x + (c.x - a.x) * (y - a.y) / (c.y - a.y);   /* long edge a-c */
    int xs = y < b.y ? (b.y == a.y ? b.x : a.x + (b.x - a.x) * (y - a.y) / (b.y - a.y))
                     : (c.y == b.y ? b.x : b.x + (c.x - b.x) * (y - b.y) / (c.y - b.y));
    graphics_draw_line(ctx, GPoint(xl, y), GPoint(xs, y));
  }
}

/* Delta wing pointing along unit heading (hx, hy) in FX: nose `len` px ahead, wingtips `len/2`
 * behind and `half` px to each side. */
static void prv_delta(GContext *ctx, GPoint p, int32_t hx, int32_t hy, int len, int half, GColor col) {
  GPoint nose = GPoint(p.x + (int)(hx * len / FX), p.y + (int)(hy * len / FX));
  GPoint back = GPoint(p.x - (int)(hx * len / 2 / FX), p.y - (int)(hy * len / 2 / FX));
  GPoint l = GPoint(back.x - (int)(hy * half / FX), back.y + (int)(hx * half / FX));
  GPoint r = GPoint(back.x + (int)(hy * half / FX), back.y - (int)(hx * half / FX));
  graphics_context_set_fill_color(ctx, col);
  graphics_context_set_stroke_color(ctx, col);
  prv_fill_tri(ctx, nose, l, r);
}

/* Each type has its own silhouette, so the raid reads at a glance without sound:
 * Switchblade a small cross, Shahed-136 a delta, Shahed-238 a bigger delta with a jet flame,
 * Harpy a long pink dart, identified Gerbera a gray ring (unidentified it passes for a 136). */
static void prv_drone(GContext *ctx, const Game *g, const Camera *c, int d) {
  const Drone *dr = &g->drones[d];
  int type = dr->type;
  if (type == DR_GERBERA && !drone_identified(g, d)) type = DR_SHAHED136;
  GPoint p = prv_vec(c, dr->pos);
  int z = c->zoom;
  if (type == DR_GERBERA) {
    graphics_context_set_stroke_color(ctx, COL_DECOY);
    graphics_draw_circle(ctx, p, 2 * z + (z == 1));
    return;
  }
  /* Heading along the track; trail: a short segment behind the drone. */
  Vec w = dr->wps[dr->wp_idx];
  int32_t dx = w.x - dr->track.x, dy = w.y - dr->track.y;
  int32_t len = fx_dist(dx, dy);
  int32_t hx = len > 0 ? dx * FX / len : 0, hy = len > 0 ? dy * FX / len : FX;
  GPoint t = GPoint(p.x - (int)(hx * TRAIL_UNITS * z / FX), p.y - (int)(hy * TRAIL_UNITS * z / FX));
  graphics_context_set_stroke_color(ctx, COL_DRONE_TRAIL);
  graphics_draw_line(ctx, p, t);
  switch (type) {
    case DR_SWITCHBLADE:
      graphics_context_set_stroke_color(ctx, COL_DRONE);
      graphics_draw_line(ctx, GPoint(p.x - 2 * z, p.y), GPoint(p.x + 2 * z, p.y));
      graphics_draw_line(ctx, GPoint(p.x, p.y - 2 * z), GPoint(p.x, p.y + 2 * z));
      break;
    case DR_HARPY:
      prv_delta(ctx, p, hx, hy, 7 * z, 3 * z, COL_HARPY);
      break;
    case DR_SHAHED238:
      prv_delta(ctx, p, hx, hy, 6 * z, 4 * z, COL_DRONE);
      graphics_context_set_fill_color(ctx, COL_MISSILE);   /* jet exhaust */
      graphics_fill_circle(ctx, GPoint(p.x - (int)(hx * 4 * z / FX), p.y - (int)(hy * 4 * z / FX)), z);
      break;
    default:   /* DR_SHAHED136 and unidentified decoys */
      prv_delta(ctx, p, hx, hy, 5 * z, 3 * z, COL_DRONE);
      break;
  }
}

/* Explosion by phase: white-hot flash, fireball, burning ring, then a fading ring; debris flies
 * out on `rays` spokes. `size` is the fireball radius in px. */
static void prv_blast(GContext *ctx, GPoint p, int left_ms, int total_ms, int size, int rays) {
  if (left_ms < 0) left_ms = 0;
  int f = (total_ms - left_ms) * 100 / total_ms;   /* 0..100 elapsed */
  if (f < 20) {
    graphics_context_set_fill_color(ctx, GColorYellow);
    graphics_fill_circle(ctx, p, size * (40 + 3 * f) / 100);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, p, size / 3);
  } else if (f < 50) {
    graphics_context_set_fill_color(ctx, GColorOrange);
    graphics_fill_circle(ctx, p, size);
    graphics_context_set_fill_color(ctx, GColorYellow);
    graphics_fill_circle(ctx, p, size / 2);
  } else if (f < 75) {
    graphics_context_set_fill_color(ctx, GColorRed);
    graphics_fill_circle(ctx, p, size * 4 / 5);
    graphics_context_set_stroke_color(ctx, GColorOrange);
    graphics_draw_circle(ctx, p, size + 1);
  } else {
    graphics_context_set_stroke_color(ctx, GColorDarkCandyAppleRed);
    graphics_draw_circle(ctx, p, size + 2);
  }
  if (f < 20) return;
  static const int8_t DIRS[8][2] = {{7, 7}, {-7, 7}, {7, -7}, {-7, -7}, {10, 0}, {-10, 0}, {0, 10}, {0, -10}};
  int dist = size * (50 + 12 * f / 10) / 100;
  graphics_context_set_fill_color(ctx, f < 60 ? GColorOrange : GColorDarkGray);
  for (int k = 0; k < rays; k++) {
    GPoint d = GPoint(p.x + DIRS[k][0] * dist / 10, p.y + DIRS[k][1] * dist / 10);
    graphics_fill_rect(ctx, GRect(d.x, d.y, 2, 2), 0, GCornerNone);
  }
}

/* "+$N" drifting up from a downed drone, dimming at the end; black drop shadow for contrast. */
static void prv_bounty(GContext *ctx, const Camera *c, const Effect *ef) {
  int left = ef->ttl_ms < 0 ? 0 : ef->ttl_ms;
  int el = BOUNTY_MS - left;
  GPoint p = prv_vec(c, ef->a);
  p.y -= (4 + BOUNTY_RISE_UNITS * el / BOUNTY_MS) * c->zoom;
  char buf[8];
  snprintf(buf, sizeof(buf), "+$%u", (unsigned)ef->value);
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  GRect box = GRect(p.x - 20, p.y - 11, 40, 16);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, buf, font, GRect(box.origin.x + 1, box.origin.y + 1, box.size.w, box.size.h),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, el * 10 < BOUNTY_MS * 7 ? GColorYellow : GColorChromeYellow);
  graphics_draw_text(ctx, buf, font, box, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

void render_entities(GContext *ctx, const Game *g, const Camera *c) {
  static int32_t s_ranges[MAX_TOWERS];   /* once per frame, not per drone */
  combat_engage_ranges(g, s_ranges);
  for (int d = 0; d < MAX_DRONES; d++)
    if (g->drones[d].active && drone_visible_in(g, d, s_ranges)) prv_drone(ctx, g, c, d);
  for (int m = 0; m < MAX_MISSILES; m++) {
    const Missile *ms = &g->missiles[m];
    if (!ms->active) continue;
    GPoint p = prv_vec(c, ms->pos);
    const Drone *dr = &g->drones[ms->drone_idx];
    int32_t dx = dr->pos.x - ms->pos.x, dy = dr->pos.y - ms->pos.y, len = fx_dist(dx, dy);
    if (len > 0) {   /* smoke trail behind the warhead, away from its target */
      graphics_context_set_stroke_color(ctx, COL_MISSILE_TRAIL);
      graphics_draw_line(ctx, p, GPoint(p.x - (int)(dx * MISSILE_TRAIL_UNITS * c->zoom / len),
                                        p.y - (int)(dy * MISSILE_TRAIL_UNITS * c->zoom / len)));
    }
    graphics_context_set_fill_color(ctx, COL_MISSILE);
    graphics_fill_circle(ctx, p, MISSILE_UNITS * c->zoom);
  }
  for (int e = 0; e < MAX_EFFECTS; e++) {
    const Effect *ef = &g->effects[e];
    if (!ef->active) continue;
    switch (ef->kind) {
      case FX_TRACER:
        graphics_context_set_stroke_color(ctx, COL_MISSILE);
        graphics_draw_line(ctx, prv_vec(c, ef->a), prv_vec(c, ef->b));
        if (ef->ttl_ms * 2 > TRACER_MS) {   /* muzzle flash for the first half of the tracer */
          graphics_context_set_fill_color(ctx, GColorWhite);
          graphics_fill_circle(ctx, prv_vec(c, ef->a), c->zoom + 1);
        }
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
      case FX_IMPACT:
        prv_blast(ctx, prv_vec(c, ef->a), ef->ttl_ms, IMPACT_MS, IMPACT_UNITS * c->zoom, 8);
        break;
      case FX_BOUNTY:
        break;   /* text goes on top of everything, below */
      case FX_SPAWN:
        break;   /* edge blips are the night screen's: it knows where its HUD is */
      default:   /* FX_EXPLOSION */
        prv_blast(ctx, prv_vec(c, ef->a), ef->ttl_ms, EFFECT_MS, EXPLOSION_UNITS * c->zoom, 4);
        break;
    }
  }
  for (int e = 0; e < MAX_EFFECTS; e++)
    if (g->effects[e].active && g->effects[e].kind == FX_BOUNTY) prv_bounty(ctx, c, &g->effects[e]);
}

/* Inward-pointing chevron pair on edge `side` (0 top, 1 right, 2 bottom, 3 left) of rect `r`. */
static void prv_raid_arrow(GContext *ctx, GRect r, int side) {
  static const int8_t IN[4][2] = {{0, 1}, {-1, 0}, {0, -1}, {1, 0}};   /* inward unit vector */
  int ix = IN[side][0], iy = IN[side][1];
  int cx = r.origin.x + r.size.w / 2, cy = r.origin.y + r.size.h / 2;
  GPoint edge = side == 0 ? GPoint(cx, r.origin.y + 2) : side == 1 ? GPoint(r.origin.x + r.size.w - 3, cy)
              : side == 2 ? GPoint(cx, r.origin.y + r.size.h - 3) : GPoint(r.origin.x + 2, cy);
  graphics_context_set_fill_color(ctx, GColorRed);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  for (int k = 0; k < 2; k++) {
    GPoint b = GPoint(edge.x + ix * k * (RAID_ARROW_D - 1), edge.y + iy * k * (RAID_ARROW_D - 1));
    int hw = RAID_ARROW_W / 2;
    GPoint pts[3] = {GPoint(b.x - iy * hw, b.y - ix * hw), GPoint(b.x + iy * hw, b.y + ix * hw),
                     GPoint(b.x + ix * RAID_ARROW_D, b.y + iy * RAID_ARROW_D)};
    GPath path = {3, pts, 0, GPointZero};
    gpath_draw_filled(ctx, &path);
    gpath_draw_outline(ctx, &path);
  }
}

/* Where tonight's drones come from: the same sides night_begin will generate. */
void render_raid_arrows(GContext *ctx, const Game *g, GRect r) {
  uint8_t sides[4];
  int n = wave_sides(g->night, g->seed, sides);
  for (int i = 0; i < n; i++) prv_raid_arrow(ctx, r, sides[i]);
}
