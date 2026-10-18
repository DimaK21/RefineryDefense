#include "app.h"
#include "report_window.h"
#include "storage.h"
#include "ui.h"
#include "ui_colors.h"
#include "../logic/sim.h"
#include "../logic/config.h"

#define INPUT_GUARD_MS 600   /* UI tuning: input this soon after the screen appears is ignored */

static Window *s_window;
static AppTimer *s_guard_timer;
static bool s_ready;   /* guard elapsed */
static bool s_down;    /* a touch started after the guard: its liftoff dismisses */
static Layer *s_layer;

#define ROW_H 21
#define ROWS 6
#define ROW_MARGIN 12

static void prv_ack(void) {
  Game *g = app_game();
  report_ack(g);
  storage_save(g, false);
  app_go_day();
}

static void prv_guard_done(void *ctx) {
  s_guard_timer = NULL;
  s_ready = true;
  layer_mark_dirty(s_layer);   /* show the "tap to continue" hint */
}

static void prv_update(Layer *layer, GContext *ctx) {
  const Game *g = app_game();
  const NightStats *st = &g->stats;
  GRect b = layer_get_bounds(layer);
  char kicker[16], v[ROWS][16], cash[16];
  snprintf(kicker, sizeof(kicker), "NIGHT %u", (unsigned)nights_survived(g));
  ui_background(ctx, b);
  ui_header(ctx, b, UI_HDR_GOOD, kicker, "SURVIVED");
  snprintf(v[0], sizeof(v[0]), "%u", (unsigned)st->kills);
  snprintf(v[1], sizeof(v[1]), "+$%ld", (long)st->bounty);
  snprintf(v[2], sizeof(v[2]), "+$%ld", (long)st->income);
  snprintf(v[3], sizeof(v[3]), "%u", (unsigned)st->objects_hit);
  snprintf(v[4], sizeof(v[4]), "%u", (unsigned)st->towers_lost);
  snprintf(v[5], sizeof(v[5]), "%u", (unsigned)st->best_combo);
  const char *labels[ROWS] = {"Drones downed", "Bounty", "Plant income", "Plant hits", "Towers lost",
                              "Best combo"};
  GColor colors[ROWS] = {UI_TEXT, UI_ACCENT, UI_GOOD, st->objects_hit ? UI_BAD : UI_TEXT,
                         st->towers_lost ? UI_BAD : UI_TEXT,
                         st->best_combo >= COMBO_X15_AT ? UI_ACCENT : UI_TEXT};
  int w = b.size.w - 2 * ROW_MARGIN, y = UI_HEADER_H + 6;
  for (int i = 0; i < ROWS; i++, y += ROW_H)
    ui_stat_row(ctx, GRect(ROW_MARGIN, y, w, ROW_H), labels[i], v[i], colors[i]);
  snprintf(cash, sizeof(cash), "$%ld", (long)g->money);
  y += 4;
  ui_text(ctx, "Cash for the day", FONT_KEY_GOTHIC_18_BOLD, GRect(ROW_MARGIN, y, w, 24),
          GTextAlignmentLeft, UI_TEXT);
  ui_text(ctx, cash, FONT_KEY_GOTHIC_24_BOLD, GRect(ROW_MARGIN, y - 5, w, 28), GTextAlignmentRight, UI_ACCENT);
  if (s_ready) ui_footer(ctx, b, "Tap to continue");
}

static void prv_click(ClickRecognizerRef r, void *ctx) {
  if (s_ready) prv_ack();
}

static void prv_clicks(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_click);
  window_single_click_subscribe(BUTTON_ID_UP, prv_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_click);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_click);
}

/* Dismiss on a completed tap, so taps still meant for the night screen do not skip this one. */
static void prv_touch(const TouchEvent *e, void *ctx) {
  if (!s_ready) return;
  if (e->type == TouchEvent_Touchdown) s_down = true;
  else if (e->type == TouchEvent_Liftoff && s_down) { s_down = false; prv_ack(); }
}

static void prv_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, prv_update);
  layer_add_child(root, s_layer);
  s_ready = false;
  s_down = false;
  s_guard_timer = app_timer_register(INPUT_GUARD_MS, prv_guard_done, NULL);
  touch_service_subscribe(prv_touch, NULL);
}

static void prv_unload(Window *w) {
  if (s_guard_timer) { app_timer_cancel(s_guard_timer); s_guard_timer = NULL; }
  touch_service_unsubscribe();
  layer_destroy(s_layer);
  s_layer = NULL;
}

void report_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, UI_BG);
    window_set_click_config_provider(s_window, prv_clicks);
    window_set_window_handlers(s_window, (WindowHandlers){ .load = prv_load, .unload = prv_unload });
  }
  window_stack_push(s_window, false);
}

void report_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
