#include "app.h"
#include "menu_window.h"
#include "storage.h"
#include "../logic/economy.h"
#include "../logic/sim.h"

static Window *s_window;
static MenuLayer *s_menu;
static bool s_has_continue;
static char s_best[40];

static void prv_refresh(void) {
  s_has_continue = storage_exists();
  Best b = storage_best();
  snprintf(s_best, sizeof(s_best), "Best: %u nights \xC2\xB7 %lu pts",
           (unsigned)b.nights, (unsigned long)b.score);
}

static uint16_t prv_rows(MenuLayer *m, uint16_t section, void *ctx) {
  return s_has_continue ? 3 : 2;
}

static void prv_draw(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  int row = idx->row;
  if (!s_has_continue) row++;  /* row 0 = Continue, 1 = New Game, 2 = Best */
  if (row == 0) menu_cell_basic_draw(ctx, cell, "Continue", NULL, NULL);
  else if (row == 1) menu_cell_basic_draw(ctx, cell, "New Game", NULL, NULL);
  else menu_cell_basic_draw(ctx, cell, s_best, NULL, NULL);
}

static void prv_select(MenuLayer *m, MenuIndex *idx, void *data) {
  int row = idx->row;
  if (!s_has_continue) row++;
  Game *g = app_game();
  if (row == 0) {
    bool pending = false;
    if (!storage_load(g, &pending)) { prv_refresh(); menu_layer_reload_data(s_menu); return; }
    if (pending) {
      night_begin(g);
      app_go_night();
    } else {
      app_go_day();
    }
  } else if (row == 1) {
    uint32_t seed = (uint32_t)time(NULL) ^ 0x5EEDC0DEu;
    if (seed == 0) seed = 1;
    game_new(g, seed);
    storage_save(g, false);
    app_go_day();
  }
}

static void prv_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = prv_rows,
    .draw_row = prv_draw,
    .select_click = prv_select,
  });
  menu_layer_set_click_config_onto_window(s_menu, w);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void prv_appear(Window *w) {
  prv_refresh();
  menu_layer_reload_data(s_menu);
}

static void prv_unload(Window *w) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
}

void menu_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = prv_load, .appear = prv_appear, .unload = prv_unload });
  }
  window_stack_push(s_window, false);
}

void menu_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
