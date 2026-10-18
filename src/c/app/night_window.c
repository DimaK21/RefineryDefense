#include "app.h"
#include "night_window.h"
#include "gameover_window.h"
#include "feedback.h"
#include "render.h"
#include "storage.h"
#include "ui_colors.h"
#include "../logic/sim.h"
#include "../logic/config.h"
#include "../logic/economy.h"
#include "../logic/fx.h"
#include "../logic/gesture.h"
#include "../logic/camera.h"

#define TICK_MS 40
#define MAX_CATCHUP_MS 200   /* real ms simulated per callback at most: a stall must not teleport drones */
#define HUD_H 14
#define MARK_CONFIRM_PX 20   /* UI tuning: a tap this close to the marker confirms Air Support */
#define MARK_TTL_MS 3000     /* UI tuning: the marker disappears this long after its last placement */

static Window *s_window;
static Layer *s_layer;
static AppTimer *s_timer;
static const Camera s_cam = {0, 0, 1};
static GestureTracker s_gt;
static TouchClock s_clock;
static uint32_t s_last_ms;
static bool s_fast, s_paused;
static bool s_mark;
static GPoint s_mark_pt;
static uint32_t s_mark_ms;

static uint32_t prv_now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return touch_clock_ms(&s_clock, (uint32_t)s, ms);
}

static bool prv_air_available(const Game *g) {
  return !g->air_used && g->money >= AIR_COST;
}

/* ---- drawing ---- */

static void prv_text(GContext *ctx, const char *txt, GRect r, GTextAlignment align, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, txt, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), r,
                     GTextOverflowModeTrailingEllipsis, align, NULL);
}

static void prv_air_icon(GContext *ctx, GPoint c, GColor col) {
  graphics_context_set_stroke_color(ctx, col);
  graphics_draw_line(ctx, GPoint(c.x, c.y - 5), GPoint(c.x, c.y + 4));           /* fuselage */
  graphics_draw_line(ctx, GPoint(c.x - 5, c.y), GPoint(c.x + 5, c.y));           /* wings */
  graphics_draw_line(ctx, GPoint(c.x - 4, c.y + 1), GPoint(c.x + 4, c.y + 1));
  graphics_draw_line(ctx, GPoint(c.x - 2, c.y + 4), GPoint(c.x + 2, c.y + 4));   /* tail */
}

static void prv_draw_hud(GContext *ctx, const Game *g, int w) {
  /* No alpha on Pebble: black on every other row lets the map show through. */
  graphics_context_set_stroke_color(ctx, COL_HUD_BG);
  for (int y = 0; y < HUD_H; y += 2) graphics_draw_line(ctx, GPoint(0, y), GPoint(w - 1, y));
  char buf[16];
  snprintf(buf, sizeof(buf), "$%ld", (long)g->money);
  prv_text(ctx, buf, GRect(3, -3, 50, HUD_H + 4), GTextAlignmentLeft, COL_HUD_MONEY);
  snprintf(buf, sizeof(buf), "NIGHT %u", (unsigned)g->night);
  prv_text(ctx, buf, GRect(52, -3, 70, HUD_H + 4), GTextAlignmentCenter, COL_HUD_TEXT);
  if (s_fast) prv_text(ctx, "x2", GRect(122, -3, 18, HUD_H + 4), GTextAlignmentLeft, COL_SELECT);
  prv_air_icon(ctx, GPoint(148, HUD_H / 2), prv_air_available(g) ? COL_SELECT : COL_DISABLED);
  int hp = plant_health_pct(g);
  snprintf(buf, sizeof(buf), "%d%%", hp);
  prv_text(ctx, buf, GRect(w - 48, -3, 45, HUD_H + 4), GTextAlignmentRight, health_color(hp));
}

static void prv_draw_mark(GContext *ctx, GPoint p, GColor col) {
  graphics_context_set_stroke_color(ctx, col);
  graphics_draw_circle(ctx, p, AIR_RADIUS);
  graphics_draw_line(ctx, GPoint(p.x - 3, p.y), GPoint(p.x + 3, p.y));
  graphics_draw_line(ctx, GPoint(p.x, p.y - 3), GPoint(p.x, p.y + 3));
}

static void prv_draw_pause(GContext *ctx, GRect b) {
  GRect box = GRect(10, b.size.h / 2 - 30, b.size.w - 20, 60);
  graphics_context_set_fill_color(ctx, COL_HUD_BG);
  graphics_fill_rect(ctx, box, 4, GCornersAll);
  graphics_context_set_stroke_color(ctx, COL_HUD_TEXT);
  graphics_draw_round_rect(ctx, box, 4);
  graphics_context_set_text_color(ctx, COL_HUD_TEXT);
  graphics_draw_text(ctx, "PAUSED", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(box.origin.x, box.origin.y + 2, box.size.w, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  prv_text(ctx, "Back: quit, Select: resume", GRect(box.origin.x, box.origin.y + 32, box.size.w, 20),
           GTextAlignmentCenter, COL_HUD_TEXT);
}

static void prv_update(Layer *layer, GContext *ctx) {
  const Game *g = app_game();
  GRect b = layer_get_bounds(layer);
  render_map(ctx, g, &s_cam, true);
  render_towers(ctx, g, &s_cam, false);
  render_entities(ctx, g, &s_cam);
  if (g->air_active) prv_draw_mark(ctx, GPoint(g->air_point.x / FX, g->air_point.y / FX), COL_EXPLOSION);
  else if (s_mark && prv_air_available(g)) prv_draw_mark(ctx, s_mark_pt, COL_AIR_MARK);
  prv_draw_hud(ctx, g, b.size.w);
  if (s_paused) prv_draw_pause(ctx, b);
}

/* ---- simulation ---- */

static void prv_tick_cb(void *ctx);

static void prv_schedule(void) {
  if (s_layer && !s_paused && app_game()->phase == PHASE_NIGHT && !s_timer)
    s_timer = app_timer_register(TICK_MS, prv_tick_cb, NULL);
}

static void prv_finish_night(Game *g) {
  if (g->phase == PHASE_REPORT) {
    /* Income and night++ are already applied: the save loads as the next DAY, and the
       report's own save after report_ack writes the same state. */
    storage_save(g, false);
    app_go_report();
    return;
  }
  Best b = storage_best();
  bool record = best_update(&b, nights_survived(g), g->score);
  if (record) storage_set_best(b);
  storage_clear();
  gameover_window_set_record(record);
  app_go_gameover();
}

static void prv_tick_cb(void *ctx) {
  s_timer = NULL;
  Game *g = app_game();
  uint32_t now = prv_now_ms();
  uint32_t real = now - s_last_ms;
  s_last_ms = now;
  if (real > MAX_CATCHUP_MS) real = MAX_CATCHUP_MS;
  uint32_t sim = s_fast ? real * 2 : real;
  while (sim > 0 && g->phase == PHASE_NIGHT) {
    uint16_t step = sim > TICK_MS ? TICK_MS : (uint16_t)sim;
    sim_tick(g, step);
    sim -= step;
  }
  feedback_events(sim_take_events(g));
  if (g->phase == PHASE_REPORT || g->phase == PHASE_GAME_OVER) { prv_finish_night(g); return; }
  if (s_mark && now - s_mark_ms >= MARK_TTL_MS) s_mark = false;
  layer_mark_dirty(s_layer);
  prv_schedule();
}

static void prv_set_paused(bool paused) {
  s_paused = paused;
  if (paused) {
    if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  } else {
    s_last_ms = prv_now_ms();
    prv_schedule();
  }
  layer_mark_dirty(s_layer);
}

static void prv_focus(bool in_focus) {
  /* a notification or system overlay: the night must not run unseen */
  if (!in_focus && !s_paused && app_game()->phase == PHASE_NIGHT) prv_set_paused(true);
}

/* ---- input ---- */

static void prv_tap(int16_t x, int16_t y) {
  Game *g = app_game();
  if (s_paused || g->phase != PHASE_NIGHT || !prv_air_available(g)) return;
  if (s_mark && fx_dist(x - s_mark_pt.x, y - s_mark_pt.y) <= MARK_CONFIRM_PX) {
    Vec p = {s_mark_pt.x * FX, s_mark_pt.y * FX};
    air_support_call(g, p);
    s_mark = false;
  } else {
    s_mark = true;
    s_mark_pt = GPoint(x, y);
    s_mark_ms = prv_now_ms();
  }
  layer_mark_dirty(s_layer);
}

static void prv_touch(const TouchEvent *e, void *ctx) {
  TouchKind k = e->type == TouchEvent_Touchdown ? TOUCH_DOWN
              : e->type == TouchEvent_Liftoff ? TOUCH_UP : TOUCH_MOVE;
  Gesture gs = gesture_feed(&s_gt, k, e->x, e->y, prv_now_ms());
  if (gs.kind == GES_TAP) prv_tap(gs.x, gs.y);
}

static void prv_select(ClickRecognizerRef r, void *ctx) {
  if (s_paused) { prv_set_paused(false); return; }
  s_fast = !s_fast;
  layer_mark_dirty(s_layer);
}

static void prv_back(ClickRecognizerRef r, void *ctx) {
  if (s_paused) { app_go_menu(); return; }   /* the pending_night save from day start stays */
  prv_set_paused(true);
}

static void prv_clicks(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back);
}

/* ---- window ---- */

static void prv_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, prv_update);
  layer_add_child(root, s_layer);
  s_fast = false;
  s_paused = false;
  s_mark = false;
  s_clock = (TouchClock){0};
  s_last_ms = prv_now_ms();
  prv_schedule();
  app_focus_service_subscribe_handlers((AppFocusHandlers){ .will_focus = prv_focus });
}

static void prv_unload(Window *w) {
  app_focus_service_unsubscribe();
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  layer_destroy(s_layer);
  s_layer = NULL;
}

static void prv_appear(Window *w) {
  s_gt = (GestureTracker){0};
  touch_service_subscribe(prv_touch, NULL);
}

static void prv_disappear(Window *w) {
  touch_service_unsubscribe();
}

void night_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, COL_NIGHT_BG);
    window_set_touch_bridge_disabled(s_window, true);
    window_set_click_config_provider(s_window, prv_clicks);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = prv_load, .appear = prv_appear, .disappear = prv_disappear, .unload = prv_unload });
  }
  window_stack_push(s_window, false);
}

void night_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
