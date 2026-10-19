#include "app.h"
#include "day_window.h"
#include "day_panel.h"
#include "feedback.h"
#include "render.h"
#include "storage.h"
#include "ui_colors.h"
#include "../logic/config.h"
#include "../logic/economy.h"
#include "../logic/drones.h"
#include "../logic/fx.h"
#include "../logic/gesture.h"
#include "../logic/camera.h"
#include "../logic/sim.h"
#include "../logic/waves.h"

#define DAY_ZOOM 2
#ifdef PBL_ROUND
#define HUD_H 38          /* two rows, low enough for the circle to be wide there */
#define DOCK_Y 184        /* tower bar (or panel) on top of Play, filling the bottom of the circle */
#define PANEL_W 200       /* as wide as on a rect watch; at DOCK_Y its corners still sit inside the circle */
#define PLAY_W 80
#define PLAY_H 26
#define VIEW_INSET 42     /* camera viewport inset: its corners, i.e. the world's, stay visible */
#define SIDE_DX 60        /* minimap / Reload: this far right / left of the centre */
#else
#define HUD_H 18
#endif
#define BAR_H 28
#define SLOT_W 28
#define MINI_W 46
#define MINI_H 44
#define MINI_MARGIN 2
#define RAID_EDGE_PX 2    /* minimap edge stripe for a raid side */
#define RELOAD_W 46       /* "Reload all" button, bottom-left of the map, mirrors the minimap */
#define RELOAD_H 30
#define START_CONFIRM_MS 2000   /* UI tuning: a second Start tap / Select within this starts the night */

static Window *s_window;
static Layer *s_map_layer;      /* camera viewport; on a round screen the map is drawn past it */
static Layer *s_ui_layer;       /* HUD, bar / panel, minimap on top */
static GRect s_bounds;

static Camera s_cam;
static GestureTracker s_gt;
static TouchClock s_clock;
static int16_t s_pan_cam_x, s_pan_cam_y;
static TowerType s_sel;
static bool s_ghost;
static int s_ghost_col, s_ghost_row;
static bool s_move;
static int s_move_idx;
static bool s_start_armed;      /* first Start tap / Select arms, the second one confirms */
static AppTimer *s_start_timer;

/* ---- layout ---- */

#ifdef PBL_ROUND
/* Camera viewport: the camera clamps to it, so every world edge can be brought inside the circle. */
static GRect prv_view_rect(void) {
  return GRect(VIEW_INSET, VIEW_INSET, s_bounds.size.w - 2 * VIEW_INSET, DOCK_Y - MINI_MARGIN - VIEW_INSET);
}
/* Where a tap lands on the map: everything between HUD and dock. */
static GRect prv_map_rect(void) {
  return GRect(0, HUD_H, s_bounds.size.w, DOCK_Y - HUD_H);
}
static GRect prv_dock_rect(void) {
  return GRect(0, DOCK_Y, s_bounds.size.w, s_bounds.size.h - DOCK_Y);
}
static GRect prv_bar_rect(void) {
  return GRect((s_bounds.size.w - TW_COUNT * SLOT_W) / 2, DOCK_Y, TW_COUNT * SLOT_W, BAR_H);
}
static GRect prv_play_rect(void) {
  return GRect((s_bounds.size.w - PLAY_W) / 2, DOCK_Y + BAR_H + 4, PLAY_W, PLAY_H);
}
static GRect prv_panel_rect(void) {
  return GRect((s_bounds.size.w - PANEL_W) / 2, DOCK_Y, PANEL_W, BAR_H);
}
static GRect prv_mini_rect(void) {
  return GRect(s_bounds.size.w / 2 + SIDE_DX, DOCK_Y - MINI_H - MINI_MARGIN, MINI_W, MINI_H);
}
static GRect prv_reload_rect(void) {
  return GRect(s_bounds.size.w / 2 - SIDE_DX - RELOAD_W, DOCK_Y - RELOAD_H - MINI_MARGIN, RELOAD_W, RELOAD_H);
}
#else
static GRect prv_map_rect(void) {
  return GRect(0, HUD_H, s_bounds.size.w, s_bounds.size.h - HUD_H - BAR_H);
}
static GRect prv_view_rect(void) { return prv_map_rect(); }
static GRect prv_bar_rect(void) {
  return GRect(0, s_bounds.size.h - BAR_H, s_bounds.size.w, BAR_H);
}
static GRect prv_dock_rect(void) { return prv_bar_rect(); }
static GRect prv_play_rect(void) {
  return GRect(TW_COUNT * SLOT_W, s_bounds.size.h - BAR_H, s_bounds.size.w - TW_COUNT * SLOT_W, BAR_H);
}
static GRect prv_panel_rect(void) { return prv_bar_rect(); }
static GRect prv_mini_rect(void) {
  return GRect(s_bounds.size.w - MINI_W - MINI_MARGIN, s_bounds.size.h - BAR_H - MINI_H - MINI_MARGIN,
               MINI_W, MINI_H);
}

static GRect prv_reload_rect(void) {
  return GRect(MINI_MARGIN, s_bounds.size.h - BAR_H - RELOAD_H - MINI_MARGIN, RELOAD_W, RELOAD_H);
}
#endif

/* Only when some Tor / Patriot lacks missiles and a map tap means nothing else. */
static bool prv_reload_shown(const Game *g) {
  return !s_move && !s_start_armed && day_panel_kind() == PANEL_NONE && reload_all_cost(g) > 0;
}

static void prv_dirty(void) {
  if (!s_map_layer) return;   /* window already unloaded (e.g. the tap started the night) */
  layer_mark_dirty(s_map_layer);
  layer_mark_dirty(s_ui_layer);
}

static void prv_center_camera(void) {
  GRect m = prv_view_rect();
  Vec c = object_center(OBJ_DISTILLATION);
  s_cam.zoom = DAY_ZOOM;
  s_cam.x = c.x / FX - m.size.w / DAY_ZOOM / 2;
  s_cam.y = c.y / FX - m.size.h / DAY_ZOOM / 2;
  camera_clamp(&s_cam, m.size.w, m.size.h);
}

/* ---- drawing ---- */

static void prv_map_update(Layer *layer, GContext *ctx) {
  const Game *g = app_game();
  render_map(ctx, g, &s_cam, false);
  render_towers(ctx, g, &s_cam, true);
  if (s_ghost) render_ghost(ctx, &s_cam, s_sel, s_ghost_col, s_ghost_row);
  if (s_move) {
    const Tower *t = &g->towers[s_move_idx];
    render_cell_frame(ctx, &s_cam, t->col, t->row, 1);
  } else if (day_panel_kind() == PANEL_TOWER) {
    const Tower *t = &g->towers[day_panel_index()];
    render_cell_frame(ctx, &s_cam, t->col, t->row, 1);
  } else if (day_panel_kind() == PANEL_OBJECT) {
    const ObjectSpec *s = &OBJECT_SPECS[day_panel_index()];
    render_cell_frame(ctx, &s_cam, s->col, s->row, OBJECT_CELLS);
  }
  render_raid_arrows(ctx, g, layer_get_bounds(layer));
}

static void prv_text(GContext *ctx, const char *txt, GRect r, GTextAlignment align, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, txt, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), r,
                     GTextOverflowModeTrailingEllipsis, align, NULL);
}

static void prv_draw_hud(GContext *ctx, const Game *g) {
  char buf[16];
  int w = s_bounds.size.w;
  graphics_context_set_fill_color(ctx, COL_HUD_BG);
  graphics_fill_rect(ctx, GRect(0, 0, w, HUD_H), 0, GCornerNone);
  int hp = plant_health_pct(g);
#ifdef PBL_ROUND
  /* day on top, money and plant health either side of the centre below it */
  snprintf(buf, sizeof(buf), "DAY %u", (unsigned)g->night);
  prv_text(ctx, buf, GRect(w / 2 - 40, 4, 80, 18), GTextAlignmentCenter, COL_HUD_TEXT);
  snprintf(buf, sizeof(buf), "$%ld", (long)g->money);
  prv_text(ctx, buf, GRect(w / 2 - 76, 19, 70, 18), GTextAlignmentRight, COL_HUD_MONEY);
  snprintf(buf, sizeof(buf), "Plant %d%%", hp);
  prv_text(ctx, buf, GRect(w / 2 + 6, 19, 76, 18), GTextAlignmentLeft, health_color(hp));
#else
  snprintf(buf, sizeof(buf), "$%ld", (long)g->money);
  prv_text(ctx, buf, GRect(4, -1, w / 3, HUD_H), GTextAlignmentLeft, COL_HUD_MONEY);
  snprintf(buf, sizeof(buf), "DAY %u", (unsigned)g->night);
  prv_text(ctx, buf, GRect(w / 3, -1, w / 3, HUD_H), GTextAlignmentCenter, COL_HUD_TEXT);
  snprintf(buf, sizeof(buf), "Plant %d%%", hp);
  prv_text(ctx, buf, GRect(w * 2 / 3 - 8, -1, w / 3 + 4, HUD_H), GTextAlignmentRight, health_color(hp));
#endif
}

static void prv_draw_play(GContext *ctx, GRect r) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, r);
  graphics_context_set_stroke_color(ctx, COL_HP_FULL);
  int h = 16, x0 = r.origin.x + (r.size.w - h / 2) / 2, cy = r.origin.y + r.size.h / 2;
  for (int i = 0; i <= h / 2; i++)   /* right-pointing triangle, one column per step */
    graphics_draw_line(ctx, GPoint(x0 + i, cy - h / 2 + i), GPoint(x0 + i, cy + h / 2 - i));
}

static void prv_draw_bar(GContext *ctx, const Game *g) {
  GRect bar = prv_bar_rect();
  graphics_context_set_fill_color(ctx, COL_BAR_BG);
  graphics_fill_rect(ctx, prv_dock_rect(), 0, GCornerNone);
  GRect play = prv_play_rect();
  if (s_start_armed) {
    prv_text(ctx, "Tap again to start night", GRect(bar.origin.x, bar.origin.y + 5, TW_COUNT * SLOT_W, 18),
             GTextAlignmentCenter, COL_SELECT);
    graphics_context_set_fill_color(ctx, COL_SELECT);
    graphics_fill_rect(ctx, play, 0, GCornerNone);
    prv_draw_play(ctx, play);
    return;
  }
  if (s_move) {
    prv_text(ctx, "Tap a free cell to move", GRect(bar.origin.x, bar.origin.y + 5, bar.size.w, 18),
             GTextAlignmentCenter, COL_BAR_TEXT);
    return;
  }
  char buf[8];
  for (int t = 0; t < TW_COUNT; t++) {
    GRect slot = GRect(bar.origin.x + t * SLOT_W, bar.origin.y, SLOT_W, BAR_H);
    if (t == (int)s_sel) {
      graphics_context_set_fill_color(ctx, COL_BAR_SEL);
      graphics_fill_rect(ctx, slot, 0, GCornerNone);
    }
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_rect(ctx, slot);
    render_tower_icon(ctx, t, GRect(slot.origin.x + (SLOT_W - 12) / 2, slot.origin.y + 2, 12, 12));
    int32_t price = tower_price(g, (TowerType)t);
    snprintf(buf, sizeof(buf), "%ld", (long)price);
    prv_text(ctx, buf, GRect(slot.origin.x, slot.origin.y + 11, SLOT_W, 16), GTextAlignmentCenter,
             g->money < price ? COL_NO_MONEY : COL_BAR_TEXT);
  }
  prv_draw_play(ctx, play);
}

static void prv_draw_reload(GContext *ctx, const Game *g) {
  if (!prv_reload_shown(g)) return;
  GRect r = prv_reload_rect();
  graphics_context_set_fill_color(ctx, COL_BAR_BG);
  graphics_fill_rect(ctx, r, 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, grect_inset(r, GEdgeInsets(-1)));
  int32_t cost = reload_all_cost(g);
  char buf[12];
  snprintf(buf, sizeof(buf), "$%ld", (long)cost);
  prv_text(ctx, "Reload", GRect(r.origin.x, r.origin.y - 2, r.size.w, 16), GTextAlignmentCenter, COL_BAR_TEXT);
  prv_text(ctx, buf, GRect(r.origin.x, r.origin.y + 12, r.size.w, 16), GTextAlignmentCenter,
           g->money < cost ? COL_NO_MONEY : COL_HUD_MONEY);
}

static GPoint prv_mini_pt(GRect m, int wx, int wy) {
  return GPoint(m.origin.x + wx * m.size.w / WORLD_W, m.origin.y + wy * m.size.h / WORLD_H);
}

static void prv_draw_minimap(GContext *ctx, const Game *g) {
  GRect m = prv_mini_rect();
  graphics_context_set_fill_color(ctx, COL_MINIMAP_BG);
  graphics_fill_rect(ctx, m, 0, GCornerNone);
  uint8_t sides[4];
  int n = wave_sides(g->night, g->seed, sides);
  graphics_context_set_fill_color(ctx, GColorRed);
  for (int i = 0; i < n; i++) {
    int e = RAID_EDGE_PX;
    GRect s = sides[i] == 0 ? GRect(m.origin.x, m.origin.y, m.size.w, e)
            : sides[i] == 1 ? GRect(m.origin.x + m.size.w - e, m.origin.y, e, m.size.h)
            : sides[i] == 2 ? GRect(m.origin.x, m.origin.y + m.size.h - e, m.size.w, e)
            : GRect(m.origin.x, m.origin.y, e, m.size.h);
    graphics_fill_rect(ctx, s, 0, GCornerNone);
  }
  for (int o = 0; o < OBJ_COUNT; o++) {
    const ObjectSpec *s = &OBJECT_SPECS[o];
    GPoint a = prv_mini_pt(m, s->col * CELL, s->row * CELL);
    GPoint b = prv_mini_pt(m, (s->col + OBJECT_CELLS) * CELL, (s->row + OBJECT_CELLS) * CELL);
    GRect r = GRect(a.x, a.y, b.x - a.x, b.y - a.y);
    uint8_t st = g->objects[o].status;
    if (st == OBJ_NOT_BUILT) {
      graphics_context_set_stroke_color(ctx, COL_NOT_BUILT);
      graphics_draw_rect(ctx, r);
    } else {
      graphics_context_set_fill_color(ctx, st == OBJ_ALIVE ? object_color(o) : COL_DESTROYED);
      graphics_fill_rect(ctx, r, 0, GCornerNone);
    }
  }
  for (int i = 0; i < MAX_TOWERS; i++) {
    const Tower *t = &g->towers[i];
    if (!t->active) continue;
    GPoint p = prv_mini_pt(m, t->col * CELL, t->row * CELL);
    graphics_context_set_fill_color(ctx, tower_color(t->type));
    graphics_fill_rect(ctx, GRect(p.x, p.y, 2, 2), 0, GCornerNone);
  }
  GRect v = prv_view_rect();
  GPoint a = prv_mini_pt(m, s_cam.x, s_cam.y);
  GPoint b = prv_mini_pt(m, s_cam.x + v.size.w / s_cam.zoom, s_cam.y + v.size.h / s_cam.zoom);
  graphics_context_set_stroke_color(ctx, COL_VIEW_FRAME);
  graphics_draw_rect(ctx, GRect(a.x, a.y, b.x - a.x, b.y - a.y));
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_rect(ctx, grect_inset(m, GEdgeInsets(-1)));
}

static void prv_ui_update(Layer *layer, GContext *ctx) {
  const Game *g = app_game();
  prv_draw_hud(ctx, g);
  if (day_panel_kind() != PANEL_NONE) {
    graphics_context_set_fill_color(ctx, COL_BAR_BG);
    graphics_fill_rect(ctx, prv_dock_rect(), 0, GCornerNone);
    day_panel_draw(ctx, prv_panel_rect(), g);
  } else {
    prv_draw_bar(ctx, g);
  }
  prv_draw_minimap(ctx, g);
  prv_draw_reload(ctx, g);
}

/* ---- input ---- */

static void prv_start_night(void) {
  Game *g = app_game();
  storage_save(g, true);
  if (night_begin(g)) app_go_night();
}

static void prv_disarm_start(void) {
  if (s_start_timer) { app_timer_cancel(s_start_timer); s_start_timer = NULL; }
  s_start_armed = false;
}

static void prv_start_timeout(void *ctx) {
  s_start_timer = NULL;
  s_start_armed = false;
  prv_dirty();
}

/* Two-step start: the night cannot be undone, so one stray tap or press only arms it. */
static void prv_request_start(void) {
  if (s_start_armed) {
    prv_disarm_start();
    prv_start_night();
    return;
  }
  day_panel_close();
  s_move = false;
  s_start_armed = true;
  s_start_timer = app_timer_register(START_CONFIRM_MS, prv_start_timeout, NULL);
}

static void prv_tap_bar(int16_t x) {
  int slot = (x - prv_bar_rect().origin.x) / SLOT_W;
  if (slot < TW_COUNT) s_sel = (TowerType)slot;
  else prv_request_start();
}

static void prv_tap_cell(Game *g, int col, int row) {
  if (s_move) {
    if (cell_free(g, col, row)) tower_move(g, s_move_idx, col, row);
    s_move = false;
    return;
  }
  int t = tower_at(g, col, row);
  int o = object_at(g, col, row);
  if (t >= 0 || o >= 0) {
    s_ghost = false;
    day_panel_open(t >= 0 ? PANEL_TOWER : PANEL_OBJECT, t >= 0 ? t : o);
    return;
  }
  if (s_ghost && s_ghost_col == col && s_ghost_row == row) {
    ActResult r = tower_buy(g, s_sel, col, row);
    if (r == ACT_OK) s_ghost = false;
    else feedback_short();   /* no money (or no free tower slot): keep the ghost */
    return;
  }
  s_ghost = true;
  s_ghost_col = col;
  s_ghost_row = row;
}

static void prv_tap(int16_t x, int16_t y) {
  Game *g = app_game();
  GPoint p = GPoint(x, y);
  GRect bar = prv_bar_rect(), play = prv_play_rect(), mini = prv_mini_rect(), map = prv_map_rect();
  GRect view = prv_view_rect(), panel = prv_panel_rect();
  if (s_start_armed) {   /* only the Start button confirms; any other tap just disarms */
    if (grect_contains_point(&play, &p)) prv_request_start();
    else prv_disarm_start();
    return;
  }
  if (day_panel_kind() != PANEL_NONE) {
    if (grect_contains_point(&panel, &p)) {
      if (day_panel_tap(g, panel, x, y) == PANEL_RES_MOVE) {
        s_move = true;
        s_move_idx = day_panel_index();
      }
    } else {
      day_panel_close();
    }
    return;
  }
  GRect reload = prv_reload_rect();
  if (prv_reload_shown(g) && grect_contains_point(&reload, &p)) {
    if (reload_all(g) == ACT_NO_MONEY) feedback_short();
    s_ghost = false;
    return;
  }
  bool on_map = grect_contains_point(&map, &p) && !grect_contains_point(&mini, &p);
  if (s_move && !on_map) { s_move = false; return; }   /* bar hint, minimap, HUD: cancel */
  if (grect_contains_point(&play, &p)) { prv_request_start(); return; }
  if (grect_contains_point(&bar, &p)) { prv_tap_bar(x); return; }
  if (!on_map) return;   /* minimap, HUD */
  int16_t wx, wy;
  int col, row;
  camera_to_world(&s_cam, x - view.origin.x, y - view.origin.y, &wx, &wy);
  if (world_to_cell(wx, wy, &col, &row)) prv_tap_cell(g, col, row);
  else { s_ghost = false; s_move = false; }
}

static uint32_t prv_now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return touch_clock_ms(&s_clock, (uint32_t)s, ms);
}

static void prv_touch(const TouchEvent *e, void *ctx) {
  TouchKind k = e->type == TouchEvent_Touchdown ? TOUCH_DOWN
              : e->type == TouchEvent_Liftoff ? TOUCH_UP : TOUCH_MOVE;
  Gesture gs = gesture_feed(&s_gt, k, e->x, e->y, prv_now_ms());
  switch (gs.kind) {
    case GES_PAN_START:
      s_pan_cam_x = s_cam.x;
      s_pan_cam_y = s_cam.y;
      /* fall through */
    case GES_PAN_MOVE: {
      /* absolute from the pan origin: no drift from per-event truncation */
      GRect m = prv_view_rect();
      s_cam.x = s_pan_cam_x - (gs.x - s_gt.x0) / s_cam.zoom;
      s_cam.y = s_pan_cam_y - (gs.y - s_gt.y0) / s_cam.zoom;
      camera_clamp(&s_cam, m.size.w, m.size.h);
      prv_dirty();
      break;
    }
    case GES_TAP:
      prv_tap(gs.x, gs.y);
      prv_dirty();
      break;
    default:
      break;
  }
}

static void prv_cycle(int step) {
  prv_disarm_start();
  s_sel = (TowerType)((s_sel + TW_COUNT + step) % TW_COUNT);
  prv_dirty();
}

static void prv_up(ClickRecognizerRef r, void *ctx) { prv_cycle(-1); }
static void prv_down(ClickRecognizerRef r, void *ctx) { prv_cycle(1); }
static void prv_select(ClickRecognizerRef r, void *ctx) {
  prv_request_start();
  prv_dirty();
}

static void prv_back(ClickRecognizerRef r, void *ctx) {
  storage_save(app_game(), false);
  app_go_menu();
}

static void prv_clicks(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back);
}

/* ---- window ---- */

static void prv_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_bounds = layer_get_bounds(root);
  s_sel = TW_MG;
  s_ghost = false;
  s_move = false;
  s_start_armed = false;
  day_panel_close();
  prv_center_camera();
  s_map_layer = layer_create(prv_view_rect());
  layer_set_clips(s_map_layer, PBL_IF_ROUND_ELSE(false, true));   /* round: fill the circle around the viewport */
  layer_set_update_proc(s_map_layer, prv_map_update);
  layer_add_child(root, s_map_layer);
  s_ui_layer = layer_create(s_bounds);
  layer_set_update_proc(s_ui_layer, prv_ui_update);
  layer_add_child(root, s_ui_layer);
}

static void prv_unload(Window *w) {
  prv_disarm_start();
  /* a long-press Back exit skips prv_back: keep the day's purchases */
  if (app_game()->phase == PHASE_DAY) storage_save(app_game(), false);
  layer_destroy(s_ui_layer);
  s_ui_layer = NULL;
  layer_destroy(s_map_layer);
  s_map_layer = NULL;
}

static void prv_appear(Window *w) {
  s_gt = (GestureTracker){0};
  s_clock = (TouchClock){0};
  touch_service_subscribe(prv_touch, NULL);
}

static void prv_disappear(Window *w) {
  touch_service_unsubscribe();
}

void day_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, GColorBlack);   /* round: past the world's edge */
    window_set_touch_bridge_disabled(s_window, true);
    window_set_click_config_provider(s_window, prv_clicks);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = prv_load, .appear = prv_appear, .disappear = prv_disappear, .unload = prv_unload });
  }
  window_stack_push(s_window, false);
}

void day_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
