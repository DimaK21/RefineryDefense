#include "app.h"
#include "menu_window.h"
#include "settings_window.h"
#include "leaders_window.h"
#include "storage.h"
#include "ui.h"
#include "ui_colors.h"
#include "../logic/config.h"
#include "../logic/economy.h"
#include "../logic/sim.h"

#define BTN_W 160
#define BTN_H 26
#define BTN_GAP 4
#define BTN_MAX 4   /* Continue, New Game, Leaders, Settings */
#define TOP PBL_IF_ROUND_ELSE(24, 0)   /* round: the title moves down to where the circle is wide */
#define BTN_TOP (70 + TOP)
#define NEW_CONFIRM_MS 2500   /* UI tuning: a second New Game within this overwrites the save */

typedef enum { ITEM_CONTINUE, ITEM_NEW, ITEM_LEADERS, ITEM_SETTINGS } Item;

static Window *s_window;
static Layer *s_layer;
static Item s_items[BTN_MAX];
static int s_count;
static int s_sel;
static int s_pressed = -1;   /* button under the finger since touchdown */
static bool s_new_armed;     /* a save exists: the first New Game only asks */
static AppTimer *s_new_timer;
static bool s_picking;       /* New Game confirmed: the buttons are the difficulties */
static Difficulty s_last_diff = DIFF_NORMAL;   /* preselected in the picker */
static char s_best[40];
static char s_continue[24];

/* Bottom line: the highlighted difficulty's record in the picker, nights per difficulty otherwise. */
static void prv_refresh_best(void) {
  if (s_picking) {
    Best b = storage_best((Difficulty)s_sel);
    if (b.nights || b.score)
      snprintf(s_best, sizeof(s_best), "BEST  %u nights \xC2\xB7 %lu pts", (unsigned)b.nights,
               (unsigned long)b.score);
    else
      snprintf(s_best, sizeof(s_best), "No record yet");
    return;
  }
  Best e = storage_best(DIFF_EASY), n = storage_best(DIFF_NORMAL), h = storage_best(DIFF_HARD);
  if (e.nights || e.score || n.nights || n.score || h.nights || h.score)
    snprintf(s_best, sizeof(s_best), "BEST  E %u \xC2\xB7 N %u \xC2\xB7 H %u", (unsigned)e.nights,
             (unsigned)n.nights, (unsigned)h.nights);
  else
    snprintf(s_best, sizeof(s_best), "Survive the night.");
}

static void prv_refresh(void) {
  s_count = 0;
  if (s_picking) {
    s_count = DIFF_COUNT;
  } else {
    if (storage_exists()) {
      s_items[s_count++] = ITEM_CONTINUE;
      snprintf(s_continue, sizeof(s_continue), "Continue \xC2\xB7 %s",
               DIFFICULTY_SPECS[storage_saved_difficulty()].name);
    }
    s_items[s_count++] = ITEM_NEW;
    s_items[s_count++] = ITEM_LEADERS;
    s_items[s_count++] = ITEM_SETTINGS;
  }
  if (s_sel >= s_count) s_sel = 0;
  prv_refresh_best();
}

static void prv_set_picking(bool on) {
  s_picking = on;
  s_sel = on ? (int)s_last_diff : 0;
  prv_refresh();
  if (!on)   /* back on the main list: land on New Game */
    for (int i = 0; i < s_count; i++) if (s_items[i] == ITEM_NEW) s_sel = i;
  layer_mark_dirty(s_layer);
}

static GRect prv_btn_rect(GRect b, int i) {
  int top = BTN_TOP + (BTN_MAX - s_count) * (BTN_H + BTN_GAP) / 2;
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
  ui_text(ctx, "REFINERY", FONT_KEY_BITHAM_30_BLACK, GRect(0, 4 + TOP, b.size.w, 36), GTextAlignmentCenter,
          UI_TEXT);
  graphics_context_set_stroke_color(ctx, UI_ACCENT);
  graphics_draw_line(ctx, GPoint(28, 52 + TOP), GPoint(58, 52 + TOP));
  graphics_draw_line(ctx, GPoint(b.size.w - 59, 52 + TOP), GPoint(b.size.w - 29, 52 + TOP));
  ui_text(ctx, s_picking ? "DIFFICULTY" : "DEFENSE", FONT_KEY_GOTHIC_18_BOLD, GRect(0, 39 + TOP, b.size.w, 22),
          GTextAlignmentCenter, UI_ACCENT);
  for (int i = 0; i < s_count; i++) {
    if (s_picking) { ui_button(ctx, prv_btn_rect(b, i), DIFFICULTY_SPECS[i].name, i == s_sel, false); continue; }
    bool armed = s_items[i] == ITEM_NEW && s_new_armed;
    const char *label = s_items[i] == ITEM_CONTINUE ? s_continue
                      : s_items[i] == ITEM_SETTINGS ? "Settings"
                      : s_items[i] == ITEM_LEADERS ? "Leaders"
                      : armed ? "Overwrite save?" : "New Game";
    ui_button(ctx, prv_btn_rect(b, i), label, i == s_sel, armed);
  }
  ui_text(ctx, s_best, FONT_KEY_GOTHIC_14_BOLD, GRect(0, BTN_TOP + BTN_MAX * (BTN_H + BTN_GAP) - 1, b.size.w, 18),
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
  prv_set_picking(true);
}

static void prv_start(Difficulty d) {
  s_last_diff = d;
  s_picking = false;
  uint32_t seed = (uint32_t)time(NULL) ^ 0x5EEDC0DEu;
  if (seed == 0) seed = 1;
  Game *g = app_game();
  game_new(g, seed, d);
  storage_save(g, false);
  app_go_day();
}

static void prv_activate(int i) {
  if (i < 0 || i >= s_count) return;
  if (s_picking) { prv_start((Difficulty)i); return; }
  if (s_items[i] != ITEM_NEW) prv_disarm_new();
  switch (s_items[i]) {
    case ITEM_CONTINUE: prv_continue(); break;
    case ITEM_NEW:      prv_new_game(); break;
    case ITEM_LEADERS:
      leaders_window_push(storage_exists() ? storage_saved_difficulty() : s_last_diff);
      break;
    case ITEM_SETTINGS: settings_window_push(); break;
  }
}

/* ---- input ---- */

static void prv_move(int step) {
  prv_disarm_new();
  s_sel = (s_sel + s_count + step) % s_count;
  prv_refresh_best();
  layer_mark_dirty(s_layer);
}

static void prv_up(ClickRecognizerRef r, void *ctx) { prv_move(-1); }
static void prv_down(ClickRecognizerRef r, void *ctx) { prv_move(1); }
static void prv_select(ClickRecognizerRef r, void *ctx) { prv_activate(s_sel); }
static void prv_back(ClickRecognizerRef r, void *ctx) {
  if (s_picking) prv_set_picking(false);
  else window_stack_pop_all(false);   /* the menu is the root: leaving it quits */
}

static void prv_clicks(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back);
}

static int prv_button_at(int16_t x, int16_t y) {
  GRect b = layer_get_bounds(s_layer), rects[BTN_MAX];
  for (int i = 0; i < s_count; i++) rects[i] = prv_btn_rect(b, i);
  return ui_hit(rects, s_count, GPoint(x, y));
}

/* Highlight on touchdown, act on a liftoff over the same button. */
static void prv_touch(const TouchEvent *e, void *ctx) {
  int hit = prv_button_at(e->x, e->y);
  if (e->type == TouchEvent_Touchdown) {
    s_pressed = hit;
    if (hit >= 0 && hit != s_sel) {
      if (!s_picking && s_items[s_sel] == ITEM_NEW) prv_disarm_new();
      s_sel = hit;
      prv_refresh_best();
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
  s_picking = false;
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
