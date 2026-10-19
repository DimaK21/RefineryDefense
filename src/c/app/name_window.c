#include <pebble.h>
#include "name_window.h"
#include "feedback.h"
#include "storage.h"
#include "ui.h"
#include "ui_colors.h"
#include "../logic/board.h"

#define REPEAT_MS 100      /* UI tuning: held Up/Down steps this often */
#define LONG_MS 500        /* UI tuning: Select held this long saves */
#define SLOT_W 26
#define SLOT_H 40
#define SLOT_GAP 4
#define SLOTS_Y 92

static Window *s_window;
static Layer *s_layer;
static char s_buf[NAME_LEN + 1];
static int s_pos;
static NameDone s_done;

static GRect prv_slot_rect(GRect b, int i) {
  int x0 = (b.size.w - (NAME_LEN * SLOT_W + (NAME_LEN - 1) * SLOT_GAP)) / 2;
  return GRect(x0 + i * (SLOT_W + SLOT_GAP), SLOTS_Y, SLOT_W, SLOT_H);
}

static void prv_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  ui_background(ctx, b);
  ui_header(ctx, b, UI_HDR_INFO, "LEADERBOARD", "Your name");
  ui_text(ctx, "Up/Down: letter  Select: next", FONT_KEY_GOTHIC_14, GRect(0, UI_HEADER_H + 10, b.size.w, 18),
          GTextAlignmentCenter, UI_MUTED);
  for (int i = 0; i < NAME_LEN; i++) {
    GRect r = prv_slot_rect(b, i);
    bool cur = i == s_pos;
    graphics_context_set_fill_color(ctx, cur ? UI_ACCENT : UI_CARD);
    graphics_fill_rect(ctx, r, 4, GCornersAll);
    char ch[2] = { s_buf[i] == ' ' ? '_' : s_buf[i], '\0' };
    ui_text(ctx, ch, FONT_KEY_GOTHIC_28_BOLD, GRect(r.origin.x, r.origin.y + 2, r.size.w, 32),
            GTextAlignmentCenter, cur ? GColorBlack : (s_buf[i] == ' ' ? UI_MUTED : UI_TEXT));
    if (cur) {   /* arrows: Up/Down change this letter */
      int cx = r.origin.x + r.size.w / 2;
      graphics_context_set_fill_color(ctx, UI_ACCENT);
      GPathInfo up = { 3, (GPoint[]){ {cx - 6, r.origin.y - 4}, {cx + 6, r.origin.y - 4}, {cx, r.origin.y - 11} } };
      GPathInfo dn = { 3, (GPoint[]){ {cx - 6, r.origin.y + r.size.h + 4}, {cx + 6, r.origin.y + r.size.h + 4},
                                      {cx, r.origin.y + r.size.h + 11} } };
      GPath *p = gpath_create(&up);
      gpath_draw_filled(ctx, p);
      gpath_destroy(p);
      p = gpath_create(&dn);
      gpath_draw_filled(ctx, p);
      gpath_destroy(p);
    }
  }
  ui_text(ctx, "Back: previous letter", FONT_KEY_GOTHIC_14, GRect(0, SLOTS_Y + SLOT_H + 20, b.size.w, 18),
          GTextAlignmentCenter, UI_MUTED);
  ui_footer(ctx, b, "Hold Select: save");
}

static void prv_close(bool saved) {
  NameDone done = s_done;
  s_done = NULL;
  window_stack_remove(s_window, true);
  if (done) done(saved);
}

static void prv_save(void) {
  char name[NAME_LEN + 1];
  memcpy(name, s_buf, sizeof(name));
  name_trim(name);
  if (!name_valid(name)) { feedback_short(); return; }   /* all blank: stay */
  storage_set_name(name);
  prv_close(true);
}

static void prv_step(int dir) {
  s_buf[s_pos] = name_cycle(s_buf[s_pos], dir);
  layer_mark_dirty(s_layer);
}

static void prv_up(ClickRecognizerRef r, void *ctx) { prv_step(1); }
static void prv_down(ClickRecognizerRef r, void *ctx) { prv_step(-1); }

static void prv_select(ClickRecognizerRef r, void *ctx) {
  if (s_pos == NAME_LEN - 1) { prv_save(); return; }
  s_pos++;
  layer_mark_dirty(s_layer);
}

static void prv_select_long(ClickRecognizerRef r, void *ctx) { prv_save(); }

static void prv_back(ClickRecognizerRef r, void *ctx) {
  if (s_pos == 0) { prv_close(false); return; }
  s_pos--;
  layer_mark_dirty(s_layer);
}

static void prv_clicks(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, REPEAT_MS, prv_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, REPEAT_MS, prv_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_long_click_subscribe(BUTTON_ID_SELECT, LONG_MS, prv_select_long, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back);
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

void name_window_push(NameDone done) {
  s_done = done;
  s_pos = 0;
  char cur[NAME_LEN + 1];
  memset(s_buf, ' ', NAME_LEN);
  s_buf[NAME_LEN] = '\0';
  if (storage_name(cur)) memcpy(s_buf, cur, strlen(cur));
  else s_buf[0] = 'A';
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, UI_BG);
    window_set_touch_bridge_disabled(s_window, true);
    window_set_click_config_provider(s_window, prv_clicks);
    window_set_window_handlers(s_window, (WindowHandlers){ .load = prv_load, .unload = prv_unload });
  }
  window_stack_push(s_window, true);
}

void name_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
