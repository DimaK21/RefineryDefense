#include "app.h"
#include "gameover_window.h"
#include "storage.h"
#include "../logic/sim.h"

#define INPUT_GUARD_MS 600   /* UI tuning: input this soon after the screen appears is ignored */

static Window *s_window;
static AppTimer *s_guard_timer;
static bool s_ready;   /* guard elapsed */
static bool s_down;    /* a touch started after the guard: its liftoff dismisses */
static TextLayer *s_text;
static char s_buf[128];
static bool s_record;

void gameover_window_set_record(bool record) { s_record = record; }

static void prv_go(void) { app_go_menu(); }
static void prv_guard_done(void *ctx) {
  s_guard_timer = NULL;
  s_ready = true;
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
  GRect b = layer_get_bounds(root);
  const Game *g = app_game();
  Best best = storage_best();
  snprintf(s_buf, sizeof(s_buf), "GAME OVER\nNights survived: %u\nScore: %lu\nBest: %u nights, %lu pts%s",
           (unsigned)nights_survived(g), (unsigned long)g->score,
           (unsigned)best.nights, (unsigned long)best.score,
           s_record ? "\nNEW RECORD" : "");
  s_text = text_layer_create(GRect(8, 8, b.size.w - 16, b.size.h - 16));
  text_layer_set_font(s_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_text, s_buf);
  layer_add_child(root, text_layer_get_layer(s_text));
  s_ready = false;
  s_down = false;
  s_guard_timer = app_timer_register(INPUT_GUARD_MS, prv_guard_done, NULL);
  touch_service_subscribe(prv_touch, NULL);
}

static void prv_unload(Window *w) {
  if (s_guard_timer) { app_timer_cancel(s_guard_timer); s_guard_timer = NULL; }
  touch_service_unsubscribe();
  text_layer_destroy(s_text);
  s_text = NULL;
}

void gameover_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_click_config_provider(s_window, prv_clicks);
    window_set_window_handlers(s_window, (WindowHandlers){ .load = prv_load, .unload = prv_unload });
  }
  window_stack_push(s_window, false);
}

void gameover_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
