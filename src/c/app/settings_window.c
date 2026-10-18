#include <pebble.h>
#include "settings_window.h"
#include "settings.h"
#include "feedback.h"
#include "ui.h"
#include "ui_colors.h"

#define ROW_H 52
#define ROW_GAP 8
#define ROW_MARGIN 8
#define TOGGLE_W 38
#define TOGGLE_H 20

typedef enum { ROW_SOUND, ROW_VIBE, ROW_COUNT } Row;

static Window *s_window;
static Layer *s_layer;
static int s_sel;
static int s_pressed = -1;

static GRect prv_row_rect(GRect b, int i) {
  return GRect(ROW_MARGIN, UI_HEADER_H + 12 + i * (ROW_H + ROW_GAP), b.size.w - 2 * ROW_MARGIN, ROW_H);
}

static void prv_draw_row(GContext *ctx, GRect r, const char *title, const char *sub, bool on,
                         bool selected) {
  graphics_context_set_fill_color(ctx, UI_CARD);
  graphics_fill_rect(ctx, r, 6, GCornersAll);
  if (selected) {
    graphics_context_set_stroke_color(ctx, UI_ACCENT);
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_round_rect(ctx, grect_inset(r, GEdgeInsets(1)), 6);
    graphics_context_set_stroke_width(ctx, 1);
  }
  int text_w = r.size.w - TOGGLE_W - 22;
  ui_text(ctx, title, FONT_KEY_GOTHIC_24_BOLD, GRect(r.origin.x + 10, r.origin.y + 1, text_w, 28),
          GTextAlignmentLeft, UI_TEXT);
  ui_text(ctx, sub, FONT_KEY_GOTHIC_14, GRect(r.origin.x + 10, r.origin.y + 29, text_w, 18),
          GTextAlignmentLeft, UI_MUTED);
  ui_toggle(ctx, GRect(r.origin.x + r.size.w - TOGGLE_W - 10, r.origin.y + (r.size.h - TOGGLE_H) / 2,
                       TOGGLE_W, TOGGLE_H), on);
}

static void prv_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  ui_background(ctx, b);
  ui_header(ctx, b, UI_HDR_INFO, NULL, "Settings");
  prv_draw_row(ctx, prv_row_rect(b, ROW_SOUND), "Sound", settings_sound() ? "Shots and blasts" : "Silent",
               settings_sound(), s_sel == ROW_SOUND);
  prv_draw_row(ctx, prv_row_rect(b, ROW_VIBE), "Vibration",
               settings_vibe() ? "Hits on the plant" : "No buzzing", settings_vibe(), s_sel == ROW_VIBE);
  ui_footer(ctx, b, "Tap a row to switch");
}

static void prv_toggle(int row) {
  if (row == ROW_SOUND) {
    settings_set_sound(!settings_sound());
    feedback_sound_preview();
  } else {
    settings_set_vibe(!settings_vibe());
    feedback_short();
  }
  layer_mark_dirty(s_layer);
}

/* ---- input ---- */

static void prv_move(int step) {
  s_sel = (s_sel + ROW_COUNT + step) % ROW_COUNT;
  layer_mark_dirty(s_layer);
}

static void prv_up(ClickRecognizerRef r, void *ctx) { prv_move(-1); }
static void prv_down(ClickRecognizerRef r, void *ctx) { prv_move(1); }
static void prv_select(ClickRecognizerRef r, void *ctx) { prv_toggle(s_sel); }

static void prv_clicks(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
}

static void prv_touch(const TouchEvent *e, void *ctx) {
  GRect b = layer_get_bounds(s_layer), rects[ROW_COUNT];
  for (int i = 0; i < ROW_COUNT; i++) rects[i] = prv_row_rect(b, i);
  int hit = ui_hit(rects, ROW_COUNT, GPoint(e->x, e->y));
  if (e->type == TouchEvent_Touchdown) {
    s_pressed = hit;
    if (hit >= 0 && hit != s_sel) { s_sel = hit; layer_mark_dirty(s_layer); }
  } else if (e->type == TouchEvent_Liftoff) {
    if (hit >= 0 && hit == s_pressed) prv_toggle(hit);
    s_pressed = -1;
  }
}

/* ---- window ---- */

static void prv_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, prv_update);
  layer_add_child(root, s_layer);
  s_sel = 0;
}

static void prv_appear(Window *w) {
  s_pressed = -1;
  touch_service_subscribe(prv_touch, NULL);
}

static void prv_disappear(Window *w) { touch_service_unsubscribe(); }

static void prv_unload(Window *w) {
  layer_destroy(s_layer);
  s_layer = NULL;
}

void settings_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, UI_BG);
    window_set_touch_bridge_disabled(s_window, true);
    window_set_click_config_provider(s_window, prv_clicks);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = prv_load, .appear = prv_appear, .disappear = prv_disappear, .unload = prv_unload });
  }
  window_stack_push(s_window, true);
}

void settings_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
