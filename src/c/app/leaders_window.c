#include <pebble.h>
#include "leaders_window.h"
#include "name_window.h"
#include "net.h"
#include "storage.h"
#include "ui.h"
#include "ui_colors.h"
#include "../logic/board.h"
#include "../logic/config.h"

#define ROW_H 16
#define LIST_TOP (UI_HEADER_H + 6)
#define LIST_X (4 + UI_ROUND_INSET)
#define LIST_ROWS 8          /* visible rows; one fewer when the unsent-best line shows */
#define LONG_MS 500          /* UI tuning: Select held this long sends */
#define MAX_ENTRIES (BOARD_MAX_ROWS + 1)   /* top rows + the player's row under a gap */

typedef enum { ST_LOADING, ST_SENDING, ST_ERROR, ST_READY } State;

static Window *s_window;
static Layer *s_layer;
static Difficulty s_diff;
static State s_state;
static const char *s_error;
static BoardRow s_entries[MAX_ENTRIES];
static int s_n;
static int s_gap_before = -1;   /* index of the player's row shown apart from the top */
static uint32_t s_my_rank;      /* 0: not on this board */
static int s_scroll;
static bool s_down;             /* a touch started on this screen */

static void prv_load_table(void);

/* The local best this board has not accepted yet. */
static bool prv_unsent(Best *out) {
  Best local = storage_best(s_diff);
  if (!best_beats(local, storage_sent_best(s_diff))) return false;
  if (out) *out = local;
  return true;
}

static int prv_visible(void) { return prv_unsent(NULL) ? LIST_ROWS - 1 : LIST_ROWS; }

static void prv_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int w = b.size.w;
  ui_background(ctx, b);
  ui_header(ctx, b, UI_HDR_INFO, "LEADERS", DIFFICULTY_SPECS[s_diff].name);
  Best unsent;
  bool has_unsent = prv_unsent(&unsent);
  if (s_state != ST_READY || s_n == 0) {
    const char *msg = s_state == ST_LOADING ? "Loading..." : s_state == ST_SENDING ? "Sending..."
                    : s_state == ST_ERROR ? s_error : "No records yet. Be the first!";
    ui_text(ctx, msg, FONT_KEY_GOTHIC_18_BOLD, GRect(10, 90, w - 20, 48), GTextAlignmentCenter,
            s_state == ST_ERROR ? UI_BAD : UI_TEXT);
  } else {
    ui_board_header(ctx, GRect(LIST_X, LIST_TOP, w - 2 * LIST_X, ROW_H));
    int vis = prv_visible();
    for (int i = 0; i < vis && s_scroll + i < s_n; i++) {
      int idx = s_scroll + i, y = LIST_TOP + ROW_H + i * ROW_H;
      if (idx == s_gap_before) {   /* dotted rule: the player's row is not next to the one above */
        graphics_context_set_stroke_color(ctx, UI_MUTED);
        for (int x = LIST_X + 4; x < w - LIST_X - 4; x += 4) graphics_draw_pixel(ctx, GPoint(x, y));
      }
      ui_board_row(ctx, GRect(LIST_X, y, w - 2 * LIST_X, ROW_H), &s_entries[idx],
                   s_my_rank && s_entries[idx].rank == s_my_rank);
    }
    if (s_scroll + vis < s_n) {   /* more below */
      graphics_context_set_fill_color(ctx, UI_MUTED);
      int y = LIST_TOP + ROW_H + vis * ROW_H + 2;
      GPathInfo dn = { 3, (GPoint[]){ {w / 2 - 5, y}, {w / 2 + 5, y}, {w / 2, y + 5} } };
      GPath *p = gpath_create(&dn);
      gpath_draw_filled(ctx, p);
      gpath_destroy(p);
    }
  }
  if (has_unsent && s_state != ST_SENDING) {
    char line[40];
    snprintf(line, sizeof(line), "Unsent best: %u nights \xC2\xB7 %lu", (unsigned)unsent.nights,
             (unsigned long)unsent.score);
    ui_text(ctx, line, FONT_KEY_GOTHIC_14_BOLD, GRect(0, b.size.h - UI_FOOTER_H - 17, w, 18),
            GTextAlignmentCenter, UI_ACCENT);
  }
  ui_footer(ctx, b, has_unsent && s_state == ST_READY ? "Select: level  Hold: send" : "Select: next level");
}

static void prv_fill(const NetReply *r) {
  s_n = 0;
  s_gap_before = -1;
  s_my_rank = r->has_me ? r->me.rank : 0;
  for (int i = 0; i < r->n_rows && s_n < MAX_ENTRIES; i++) s_entries[s_n++] = r->rows[i];
  bool me_listed = false;
  for (int i = 0; i < s_n; i++) if (s_entries[i].rank == s_my_rank) me_listed = true;
  if (r->has_me && !me_listed && s_n < MAX_ENTRIES) {
    if (s_n > 0 && s_entries[s_n - 1].rank + 1 != r->me.rank) s_gap_before = s_n;
    s_entries[s_n++] = r->me;
  }
  s_scroll = 0;
}

static void prv_table_cb(const NetReply *r) {
  if (r->status != NET_OK) {
    s_state = ST_ERROR;
    s_error = net_status_text(r->status);
  } else {
    s_state = ST_READY;
    prv_fill(r);
  }
  if (s_layer) layer_mark_dirty(s_layer);
}

static void prv_load_table(void) {
  char name[NAME_LEN + 1];
  storage_name(name);   /* "" when unset: the phone then leaves the name alone */
  s_state = ST_LOADING;
  s_n = 0;
  net_request_table(s_diff, name, prv_table_cb);
  layer_mark_dirty(s_layer);
}

static void prv_sent_cb(const NetReply *r) {
  if (r->status != NET_OK) {
    s_state = ST_ERROR;
    s_error = net_status_text(r->status);
    if (s_layer) layer_mark_dirty(s_layer);
    return;
  }
  Best local = storage_best(s_diff);
  storage_set_sent_best(s_diff, best_beats(r->online, local) ? r->online : local);
  prv_load_table();
}

static void prv_send(void) {
  Best b;
  char name[NAME_LEN + 1];
  if (!prv_unsent(&b) || !storage_name(name)) return;
  s_state = ST_SENDING;
  net_request_submit(s_diff, b, name, prv_sent_cb);
  layer_mark_dirty(s_layer);
}

static void prv_named(bool saved) {
  if (saved) prv_send();
}

/* ---- input ---- */

static void prv_next_level(void) {
  s_diff = (Difficulty)((s_diff + 1) % DIFF_COUNT);
  prv_load_table();
}

static void prv_scroll(int step) {
  if (s_state != ST_READY) return;
  int max = s_n - prv_visible();
  if (max < 0) max = 0;
  int next = s_scroll + step;
  if (next < 0 || next > max) return;
  s_scroll = next;
  layer_mark_dirty(s_layer);
}

static void prv_up(ClickRecognizerRef r, void *ctx) { prv_scroll(-1); }
static void prv_down(ClickRecognizerRef r, void *ctx) { prv_scroll(1); }
static void prv_select(ClickRecognizerRef r, void *ctx) { if (s_state != ST_SENDING) prv_next_level(); }

static void prv_select_long(ClickRecognizerRef r, void *ctx) {
  if (s_state != ST_READY || !prv_unsent(NULL)) return;
  char name[NAME_LEN + 1];
  if (storage_name(name)) prv_send();
  else name_window_push(prv_named);
}

static void prv_clicks(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_long_click_subscribe(BUTTON_ID_SELECT, LONG_MS, prv_select_long, NULL);
}

/* A completed tap switches the level, like Select. */
static void prv_touch(const TouchEvent *e, void *ctx) {
  if (e->type == TouchEvent_Touchdown) s_down = true;
  else if (e->type == TouchEvent_Liftoff && s_down) {
    s_down = false;
    if (s_state != ST_SENDING) prv_next_level();
  }
}

/* ---- window ---- */

static void prv_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, prv_update);
  layer_add_child(root, s_layer);
  prv_load_table();
}

static void prv_appear(Window *w) {
  s_down = false;
  touch_service_subscribe(prv_touch, NULL);
}

static void prv_disappear(Window *w) {
  touch_service_unsubscribe();
}

static void prv_unload(Window *w) {
  net_cancel();
  layer_destroy(s_layer);
  s_layer = NULL;
}

void leaders_window_push(Difficulty d) {
  s_diff = d < DIFF_COUNT ? d : DIFF_NORMAL;
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

void leaders_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
