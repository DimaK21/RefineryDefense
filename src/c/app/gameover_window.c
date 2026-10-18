#include "app.h"
#include "gameover_window.h"
#include "storage.h"
#include "ui.h"
#include "ui_colors.h"
#include "../logic/config.h"
#include "../logic/sim.h"

#define INPUT_GUARD_MS 600   /* UI tuning: input this soon after the screen appears is ignored */

static Window *s_window;
static AppTimer *s_guard_timer;
static bool s_ready;   /* guard elapsed */
static bool s_down;    /* a touch started after the guard: its liftoff dismisses */
static Layer *s_layer;
static bool s_record;

void gameover_window_set_record(bool record) { s_record = record; }

static void prv_go(void) { app_go_menu(); }
static void prv_guard_done(void *ctx) {
  s_guard_timer = NULL;
  s_ready = true;
  layer_mark_dirty(s_layer);   /* show the "tap to continue" hint */
}

static void prv_update(Layer *layer, GContext *ctx) {
  const Game *g = app_game();
  Best best = storage_best((Difficulty)g->difficulty);
  GRect b = layer_get_bounds(layer);
  int w = b.size.w;
  char nights[8], score[16], rec[24];
  ui_background(ctx, b);
  ui_header(ctx, b, UI_HDR_BAD, DIFFICULTY_SPECS[g->difficulty].name, "GAME OVER");
  ui_text(ctx, "The refinery has fallen", FONT_KEY_GOTHIC_14, GRect(0, UI_HEADER_H + 4, w, 18),
          GTextAlignmentCenter, UI_MUTED);
  snprintf(nights, sizeof(nights), "%u", (unsigned)nights_survived(g));
  ui_text(ctx, nights, FONT_KEY_BITHAM_42_BOLD, GRect(0, UI_HEADER_H + 16, w, 48), GTextAlignmentCenter,
          UI_TEXT);
  ui_text(ctx, "NIGHTS SURVIVED", FONT_KEY_GOTHIC_14_BOLD, GRect(0, UI_HEADER_H + 64, w, 18),
          GTextAlignmentCenter, UI_MUTED);
  int y = UI_HEADER_H + 86;
  if (s_record) {
    GRect badge = GRect((w - 116) / 2, y, 116, 22);
    graphics_context_set_fill_color(ctx, UI_ACCENT);
    graphics_fill_rect(ctx, badge, 11, GCornersAll);
    ui_text(ctx, "NEW RECORD", FONT_KEY_GOTHIC_18_BOLD, GRect(badge.origin.x, y - 3, badge.size.w, 22),
            GTextAlignmentCenter, GColorBlack);
  }
  y += 26;
  snprintf(score, sizeof(score), "%lu", (unsigned long)g->score);
  snprintf(rec, sizeof(rec), "%u nights \xC2\xB7 %lu", (unsigned)best.nights, (unsigned long)best.score);
  ui_stat_row(ctx, GRect(12, y, w - 24, 24), "Score", score, UI_ACCENT);
  ui_stat_row(ctx, GRect(12, y + 24, w - 24, 24), "Best", rec, UI_TEXT);
  if (s_ready) ui_footer(ctx, b, "Tap to continue");
}

static void prv_click(ClickRecognizerRef r, void *ctx) {
  if (s_ready) prv_go();
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
  else if (e->type == TouchEvent_Liftoff && s_down) { s_down = false; prv_go(); }
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

void gameover_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, UI_BG);
    window_set_click_config_provider(s_window, prv_clicks);
    window_set_window_handlers(s_window, (WindowHandlers){ .load = prv_load, .unload = prv_unload });
  }
  window_stack_push(s_window, false);
}

void gameover_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
