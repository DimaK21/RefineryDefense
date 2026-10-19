#include <pebble.h>
#include "app.h"
#include "submit_window.h"
#include "net.h"
#include "storage.h"
#include "ui.h"
#include "ui_colors.h"
#include "../logic/board.h"
#include "../logic/config.h"

#define ROW_H 16

typedef enum { ST_SENDING, ST_ERROR, ST_DONE } State;

static Window *s_window;
static Layer *s_layer;
static Difficulty s_diff;
static Best s_best;
static State s_state;
static const char *s_error;
static NetReply s_reply;   /* copy: net reuses its own buffer for the next request */
static bool s_better_online;
static bool s_down;

static void prv_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int w = b.size.w;
  ui_background(ctx, b);
  ui_header(ctx, b, s_state == ST_ERROR ? UI_HDR_BAD : UI_HDR_GOOD, DIFFICULTY_SPECS[s_diff].name,
            "Leaderboard");
  if (s_state != ST_DONE) {
    ui_text(ctx, s_state == ST_SENDING ? "Sending..." : s_error, FONT_KEY_GOTHIC_18_BOLD,
            GRect(10, 90, w - 20, 48), GTextAlignmentCenter, s_state == ST_ERROR ? UI_BAD : UI_TEXT);
    if (s_state == ST_ERROR)
      ui_text(ctx, "Send it later from Leaders", FONT_KEY_GOTHIC_14, GRect(10, 130, w - 20, 18),
              GTextAlignmentCenter, UI_MUTED);
  } else {
    char place[32];
    snprintf(place, sizeof(place), "#%lu of %lu", (unsigned long)s_reply.rank, (unsigned long)s_reply.total);
    ui_text(ctx, place, FONT_KEY_GOTHIC_28_BOLD, GRect(0, UI_HEADER_H + 2, w, 32), GTextAlignmentCenter,
            UI_ACCENT);
    int y = UI_HEADER_H + 36;
    if (s_better_online) {
      char line[40];
      snprintf(line, sizeof(line), "Your best online: %u nights \xC2\xB7 %lu", (unsigned)s_reply.online.nights,
               (unsigned long)s_reply.online.score);
      ui_text(ctx, line, FONT_KEY_GOTHIC_14_BOLD, GRect(0, y, w, 18), GTextAlignmentCenter, UI_TEXT);
    }
    y += 20;
    ui_board_header(ctx, GRect(4, y, w - 8, ROW_H));
    for (int i = 0; i < s_reply.n_rows; i++) {
      const BoardRow *r = &s_reply.rows[i];
      ui_board_row(ctx, GRect(4, y + ROW_H * (i + 1), w - 8, ROW_H), r, r->rank == s_reply.rank);
    }
  }
  if (s_state != ST_SENDING) ui_footer(ctx, b, "Tap to continue");
}

static void prv_sent(const NetReply *r) {
  if (r->status != NET_OK) {
    s_state = ST_ERROR;
    s_error = net_status_text(r->status);
  } else {
    s_reply = *r;
    s_better_online = best_beats(r->online, s_best);
    storage_set_sent_best(s_diff, s_better_online ? r->online : s_best);
    s_state = ST_DONE;
  }
  if (s_layer) layer_mark_dirty(s_layer);
}

static void prv_leave(void) {
  if (s_state != ST_SENDING) app_go_menu();
}

static void prv_click(ClickRecognizerRef r, void *ctx) { prv_leave(); }
/* Back while sending is the only way out of a slow send: it drops the request. */
static void prv_back(ClickRecognizerRef r, void *ctx) { app_go_menu(); }

static void prv_clicks(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_click);
  window_single_click_subscribe(BUTTON_ID_UP, prv_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_click);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back);
}

static void prv_touch(const TouchEvent *e, void *ctx) {
  if (e->type == TouchEvent_Touchdown) s_down = true;
  else if (e->type == TouchEvent_Liftoff && s_down) { s_down = false; prv_leave(); }
}

static void prv_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, prv_update);
  layer_add_child(root, s_layer);
  s_state = ST_SENDING;
  char name[NAME_LEN + 1];
  storage_name(name);
  net_request_submit(s_diff, s_best, name, prv_sent);
}

/* Touch follows visibility, like the other screens: no reliance on the order in which the
 * window below disappears and this one loads. */
static void prv_appear(Window *w) {
  s_down = false;
  touch_service_subscribe(prv_touch, NULL);
}

static void prv_disappear(Window *w) { touch_service_unsubscribe(); }

static void prv_unload(Window *w) {
  net_cancel();
  layer_destroy(s_layer);
  s_layer = NULL;
}

void submit_window_push(Difficulty d, Best b) {
  s_diff = d < DIFF_COUNT ? d : DIFF_NORMAL;
  s_best = b;
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

void submit_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
