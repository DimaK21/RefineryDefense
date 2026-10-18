#include <pebble.h>
#include "help_window.h"
#include "ui.h"
#include "ui_colors.h"

#define MARGIN 8
#define DOT_R 3
#define DOT_GAP 12

typedef struct { const char *title; const char *body; bool small; } Page;

static const Page PAGES[] = {
  {"The goal",
   "Drones raid your oil refinery every night. Keep at least one plant unit alive. "
   "Your record is how many nights you survive.", false},
  {"Day: build",
   "No timer. Pick a tower in the bottom bar or with Up/Down. Tap an empty cell to see its "
   "range, tap it again to buy. Swipe to move the map.", false},
  {"Day: manage",
   "Tap a tower or plant unit to manage it. Red arrows at the edges show where tonight's "
   "drones come from. Tap the green arrow twice to start.", false},
  {"Night",
   "Towers fire on their own. You only see drones near your towers, radars and flares.\n"
   "Quick kills in a row make a combo: x1.5 bounty from 3, x2 from 6.\n"
   "Select: x2 speed. Back: pause.", true},
  {"Abilities",
   "Up/Down: pick one. Tap the map to aim, tap the mark to use.\n"
   "Air Strike $60: wipes out drones\n"
   "EW Pulse $25: slows drones\n"
   "Flare $15: lights an area, towers in it get full range\n"
   "Each one then recharges.", true},
  {"Towers",
   "MG Nest: cheap, short range\n"
   "Gepard: solid all-rounder\n"
   "Tor, Patriot: missiles; buy reloads by day\n"
   "Jammer: slows drones down\n"
   "Radar: towers in its circle shoot at full range, outside only 60%. "
   "Reveals decoys.", true},
  {"Drones",
   "Switchblade: weak swarms\n"
   "Shahed-136: slow, big hits\n"
   "Gerbera: harmless decoy\n"
   "Harpy: hunts radars, towers\n"
   "Shahed-238: fast, very tough\n"
   "Every kill pays a bounty.", true},
  {"Money",
   "Each morning living plant units pay income by health. More units, more money, "
   "but bigger raids.", false},
};
#define PAGE_COUNT ((int)ARRAY_LENGTH(PAGES))

static Window *s_window;
static Layer *s_layer;
static int s_page;
static bool s_down;   /* a touch started on this screen: its liftoff flips the page */

static void prv_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  const Page *p = &PAGES[s_page];
  char kicker[32];
  snprintf(kicker, sizeof(kicker), "HOW TO PLAY  %d/%d", s_page + 1, PAGE_COUNT);
  ui_background(ctx, b);
  ui_header(ctx, b, UI_HDR_INFO, kicker, p->title);
  ui_text(ctx, p->body, p->small ? FONT_KEY_GOTHIC_18 : FONT_KEY_GOTHIC_24_BOLD,
          GRect(MARGIN, UI_HEADER_H + 4, b.size.w - 2 * MARGIN, b.size.h - UI_HEADER_H - UI_FOOTER_H - 4),
          GTextAlignmentLeft, UI_TEXT);
  int y = b.size.h - UI_FOOTER_H / 2, x0 = (b.size.w - (PAGE_COUNT - 1) * DOT_GAP) / 2;
  for (int i = 0; i < PAGE_COUNT; i++) {
    graphics_context_set_fill_color(ctx, i == s_page ? UI_ACCENT : GColorDarkGray);
    graphics_fill_circle(ctx, GPoint(x0 + i * DOT_GAP, y), DOT_R);
  }
}

/* Past the last page the help closes. */
static void prv_flip(int step) {
  int next = s_page + step;
  if (next >= PAGE_COUNT) { window_stack_remove(s_window, true); return; }
  if (next < 0) return;
  s_page = next;
  layer_mark_dirty(s_layer);
}

static void prv_up(ClickRecognizerRef r, void *ctx) { prv_flip(-1); }
static void prv_down_click(ClickRecognizerRef r, void *ctx) { prv_flip(1); }

static void prv_clicks(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_down_click);
}

/* Left third of the screen goes back a page, the rest goes forward. */
static void prv_touch(const TouchEvent *e, void *ctx) {
  if (e->type == TouchEvent_Touchdown) s_down = true;
  else if (e->type == TouchEvent_Liftoff && s_down) {
    s_down = false;
    prv_flip(e->x < layer_get_bounds(s_layer).size.w / 3 ? -1 : 1);
  }
}

static void prv_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, prv_update);
  layer_add_child(root, s_layer);
  s_page = 0;
}

static void prv_appear(Window *w) {
  s_down = false;
  touch_service_subscribe(prv_touch, NULL);
}

static void prv_disappear(Window *w) { touch_service_unsubscribe(); }

static void prv_unload(Window *w) {
  layer_destroy(s_layer);
  s_layer = NULL;
}

void help_window_push(void) {
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

void help_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
