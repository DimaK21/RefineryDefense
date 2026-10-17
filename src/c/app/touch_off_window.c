#include "app.h"
#include "touch_off_window.h"

static Window *s_window;
static TextLayer *s_text;

static void prv_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);
  s_text = text_layer_create(GRect(8, 60, b.size.w - 16, 100));
  text_layer_set_font(s_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_text, GTextAlignmentCenter);
  text_layer_set_text(s_text, "Touch is off. Enable touch in Settings to play.");
  layer_add_child(root, text_layer_get_layer(s_text));
}

static void prv_unload(Window *w) {
  text_layer_destroy(s_text);
  s_text = NULL;
}

void touch_off_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers){ .load = prv_load, .unload = prv_unload });
  }
  window_stack_push(s_window, false);
}

void touch_off_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
