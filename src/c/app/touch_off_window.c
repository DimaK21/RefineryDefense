#include "app.h"
#include "touch_off_window.h"
#include "ui.h"
#include "ui_colors.h"

static Window *s_window;
static Layer *s_layer;

static void prv_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  ui_background(ctx, b);
  ui_skyline(ctx, b);
  ui_header(ctx, b, UI_HDR_INFO, "REFINERY DEFENSE", "Touch is off");
  ui_text(ctx, "Turn on touch in the watch Settings to play.", FONT_KEY_GOTHIC_24_BOLD,
          GRect(12, UI_HEADER_H + 20, b.size.w - 24, 90), GTextAlignmentCenter, UI_TEXT);
}

static void prv_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, prv_update);
  layer_add_child(root, s_layer);
}

static void prv_unload(Window *w) {
  layer_destroy(s_layer);
  s_layer = NULL;
}

void touch_off_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, UI_BG);
    window_set_window_handlers(s_window, (WindowHandlers){ .load = prv_load, .unload = prv_unload });
  }
  window_stack_push(s_window, false);
}

void touch_off_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
