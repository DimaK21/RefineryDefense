#include <pebble.h>
#include "app.h"
#include "menu_window.h"
#include "touch_off_window.h"
#include "day_window.h"
#include "night_window.h"
#include "report_window.h"
#include "gameover_window.h"
#include "settings.h"
#include "settings_window.h"

static Game s_game;

Game *app_game(void) { return &s_game; }

void app_go_menu(void) { window_stack_pop_all(false); menu_window_push(); }
void app_go_day(void) { window_stack_pop_all(false); day_window_push(); }
void app_go_night(void) { window_stack_pop_all(false); night_window_push(); }
void app_go_report(void) { window_stack_pop_all(false); report_window_push(); }
void app_go_gameover(void) { window_stack_pop_all(false); gameover_window_push(); }

int main(void) {
  settings_load();
  if (touch_service_is_enabled()) {
    menu_window_push();
  } else {
    touch_off_window_push();
  }
  app_event_loop();
  window_stack_pop_all(false);
  menu_window_destroy();
  settings_window_destroy();
  touch_off_window_destroy();
  day_window_destroy();
  night_window_destroy();
  report_window_destroy();
  gameover_window_destroy();
}
