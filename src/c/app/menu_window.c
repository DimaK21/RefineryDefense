#include "app.h"
#include "menu_window.h"
#include "settings_window.h"
#include "storage.h"
#include "ui.h"
#include "ui_colors.h"
#include "../logic/economy.h"
#include "../logic/sim.h"

#define BTN_W 160
#define BTN_H 28
#define BTN_GAP 5
#define BTN_TOP 70
#define NEW_CONFIRM_MS 2500   /* UI tuning: a second New Game within this overwrites the save */

typedef enum { ITEM_CONTINUE, ITEM_NEW, ITEM_SETTINGS } Item;

static Window *s_window;
static Layer *s_layer;
static Item s_items[3];
static int s_count;
static int s_sel;
static int s_pressed = -1;   /* button under the finger since touchdown */
static bool s_new_armed;     /* a save exists: the first New Game only asks */
static AppTimer *s_new_timer;
static char s_best[40];

static void prv_refresh(void) {
  s_count = 0;
  if (storage_exists()) s_items[s_count++] = ITEM_CONTINUE;
  s_items[s_count++] = ITEM_NEW;
  s_items[s_count++] = ITEM_SETTINGS;
  if (s_sel >= s_count) s_sel = 0;
  Best b = storage_best();
  if (b.nights || b.score)
    snprintf(s_best, sizeof(s_best), "BEST  %u nights \xC2\xB7 %lu pts", (unsigned)b.nights,
             (unsigned long)b.score);
  else
    snprintf(s_best, sizeof(s_best), "Survive the night.");
}

static GRect prv_btn_rect(GRect b, int i) {
  int top = BTN_TOP + (3 - s_count) * (BTN_H + BTN_GAP) / 2;
  return GRect((b.size.w - BTN_W) / 2, top + i * (BTN_H + BTN_GAP), BTN_W, BTN_H);
}

static void prv_disarm_new(void) {
  if (s_new_timer) { app_timer_cancel(s_new_timer); s_new_timer = NULL; }
  s_new_armed = false;
}

static void prv_new_timeout(void *ctx) {
  s_new_timer = NULL;
  s_new_armed = false;
  if (s_layer) layer_mark_dirty(s_layer);
}

/* ---- drawing ---- */

static void prv_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  ui_background(ctx, b);
  ui_skyline(ctx, b);
  ui_text(ctx, "REFINERY", FONT_KEY_BITHAM_30_BLACK, GRect(0, 4, b.size.w, 36), GTextAlignmentCenter,
          UI_TEXT);
  graphics_context_set_stroke_color(ctx, UI_ACCENT);
  graphics_draw_line(ctx, GPoint(28, 52), GPoint(58, 52));
  graphics_draw_line(ctx, GPoint(b.size.w - 59, 52), GPoint(b.size.w - 29, 52));
  ui_text(ctx, "DEFENSE", FONT_KEY_GOTHIC_18_BOLD, GRect(0, 39, b.size.w, 22), GTextAlignmentCenter,
          UI_ACCENT);
  for (int i = 0; i < s_count; i++) {
    bool armed = s_items[i] == ITEM_NEW && s_new_armed;
    const char *label = s_items[i] == ITEM_CONTINUE ? "Continue"
                      : s_items[i] == ITEM_SETTINGS ? "Settings"
                      : armed ? "Overwrite save?" : "New Game";
    ui_button(ctx, prv_btn_rect(b, i), label, i == s_sel, armed);
  }
  ui_text(ctx, s_best, FONT_KEY_GOTHIC_14_BOLD, GRect(0, BTN_TOP + 3 * (BTN_H + BTN_GAP) - 2, b.size.w, 18),
          GTextAlignmentCenter, UI_MUTED);
}

/* ---- actions ---- */

static void prv_continue(void) {
  Game *g = app_game();
  bool pending = false;
  if (!storage_load(g, &pending)) {   /* corrupt save: drop it so Continue disappears */
    storage_clear();
    prv_refresh();
    layer_mark_dirty(s_layer);
    return;
  }
  if (pending) {
    night_begin(g);
    app_go_night();
  } else {
    app_go_day();
  }
}

static void prv_new_game(void) {
  if (storage_exists() && !s_new_armed) {   /* starting over would wipe the saved run */
    s_new_armed = true;
    s_new_timer = app_timer_register(NEW_CONFIRM_MS, prv_new_timeout, NULL);
    layer_mark_dirty(s_layer);
    return;
  }
  prv_disarm_new();
  uint32_t seed = (uint32_t)time(NULL) ^ 0x5EEDC0DEu;
  if (seed == 0) seed = 1;
  Game *g = app_game();
  game_new(g, seed);
  storage_save(g, false);
  app_go_day();
}

static void prv_activate(int i) {
  if (i < 0 || i >= s_count) return;
  if (s_items[i] != ITEM_NEW) prv_disarm_new();
  switch (s_items[i]) {
    case ITEM_CONTINUE: prv_continue(); break;
    case ITEM_NEW:      prv_new_game(); break;
    case ITEM_SETTINGS: settings_window_push(); break;
  }
}

/* ---- input ---- */

static void prv_move(int step) {
  prv_disarm_new();
  s_sel = (s_sel + s_count + step) % s_count;
  layer_mark_dirty(s_layer);
}

static void prv_up(ClickRecognizerRef r, void *ctx) { prv_move(-1); }
static void prv_down(ClickRecognizerRef r, void *ctx) { prv_move(1); }
static void prv_select(ClickRecognizerRef r, void *ctx) { prv_activate(s_sel); }

static void prv_clicks(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
}

static int prv_button_at(int16_t x, int16_t y) {
  GRect b = layer_get_bounds(s_layer), rects[3];
  for (int i = 0; i < s_count; i++) rects[i] = prv_btn_rect(b, i);
  return ui_hit(rects, s_count, GPoint(x, y));
}

/* Highlight on touchdown, act on a liftoff over the same button. */
static void prv_touch(const TouchEvent *e, void *ctx) {
  int hit = prv_button_at(e->x, e->y);
  if (e->type == TouchEvent_Touchdown) {
    s_pressed = hit;
    if (hit >= 0 && hit != s_sel) {
      if (s_items[s_sel] == ITEM_NEW) prv_disarm_new();
      s_sel = hit;
      layer_mark_dirty(s_layer);
    }
  } else if (e->type == TouchEvent_Liftoff) {
    int pressed = s_pressed;
    s_pressed = -1;
    if (hit >= 0 && hit == pressed) prv_activate(hit);
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
  prv_disarm_new();
  prv_refresh();
  s_pressed = -1;
  layer_mark_dirty(s_layer);
  touch_service_subscribe(prv_touch, NULL);
}

static void prv_disappear(Window *w) {
  touch_service_unsubscribe();
  prv_disarm_new();
}

static void prv_unload(Window *w) {
  layer_destroy(s_layer);
  s_layer = NULL;
}

void menu_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, UI_BG);
    window_set_touch_bridge_disabled(s_window, true);
    window_set_click_config_provider(s_window, prv_clicks);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = prv_load, .appear = prv_appear, .disappear = prv_disappear, .unload = prv_unload });
  }
  window_stack_push(s_window, false);
}

void menu_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
