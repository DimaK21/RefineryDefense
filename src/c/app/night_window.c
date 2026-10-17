#include "app.h"
#include "night_window.h"
#include "gameover_window.h"
#include "storage.h"
#include "../logic/sim.h"
#include "../logic/config.h"

#define TICK_MS 40

static Window *s_window;
static TextLayer *s_text;
static AppTimer *s_timer;
static char s_buf[48];

static void prv_update_text(void) {
  const Game *g = app_game();
  int drones = 0;
  for (int i = 0; i < MAX_DRONES; i++) if (g->drones[i].active) drones++;
  snprintf(s_buf, sizeof(s_buf), "NIGHT %u\nDrones: %d", (unsigned)g->night, drones);
  text_layer_set_text(s_text, s_buf);
}

static void prv_tick(void *ctx) {
  s_timer = NULL;
  Game *g = app_game();
  sim_tick(g, TICK_MS);
  sim_take_events(g);
  if (g->phase == PHASE_REPORT) { app_go_report(); return; }
  if (g->phase == PHASE_GAME_OVER) {
    Best b = storage_best();
    bool record = best_update(&b, nights_survived(g), g->score);
    if (record) storage_set_best(b);
    storage_clear();
    gameover_window_set_record(record);
    app_go_gameover();
    return;
  }
  prv_update_text();
  s_timer = app_timer_register(TICK_MS, prv_tick, NULL);
}

static void prv_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  GRect b = layer_get_bounds(root);
  s_text = text_layer_create(GRect(0, 70, b.size.w, 80));
  text_layer_set_font(s_text, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_text, GTextAlignmentCenter);
  layer_add_child(root, text_layer_get_layer(s_text));
  prv_update_text();
  s_timer = app_timer_register(TICK_MS, prv_tick, NULL);
}

static void prv_unload(Window *w) {
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  text_layer_destroy(s_text);
  s_text = NULL;
}

void night_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers){ .load = prv_load, .unload = prv_unload });
  }
  window_stack_push(s_window, false);
}

void night_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
