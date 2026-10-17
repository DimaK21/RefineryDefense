#include "app.h"
#include "day_window.h"
#include "storage.h"
#include "../logic/sim.h"

static Window *s_window;
static TextLayer *s_text;
static char s_buf[40];

static void prv_select(ClickRecognizerRef r, void *ctx) {
  Game *g = app_game();
  storage_save(g, true);
  if (night_begin(g)) app_go_night();
}

static void prv_back(ClickRecognizerRef r, void *ctx) {
  storage_save(app_game(), false);
  app_go_menu();
}

static void prv_clicks(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back);
}

static void prv_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);
  s_text = text_layer_create(GRect(0, 80, b.size.w, 60));
  text_layer_set_font(s_text, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_text, GTextAlignmentCenter);
  const Game *g = app_game();
  snprintf(s_buf, sizeof(s_buf), "DAY %u  $%ld", (unsigned)g->night, (long)g->money);
  text_layer_set_text(s_text, s_buf);
  layer_add_child(root, text_layer_get_layer(s_text));
}

static void prv_unload(Window *w) {
  text_layer_destroy(s_text);
  s_text = NULL;
}

void day_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_click_config_provider(s_window, prv_clicks);
    window_set_window_handlers(s_window, (WindowHandlers){ .load = prv_load, .unload = prv_unload });
  }
  window_stack_push(s_window, false);
}

void day_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
