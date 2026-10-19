#include "app.h"
#include "night_window.h"
#include "gameover_window.h"
#include "feedback.h"
#include "render.h"
#include "storage.h"
#include "ui_colors.h"
#include "../logic/sim.h"
#include "../logic/config.h"
#include "../logic/economy.h"
#include "../logic/fx.h"
#include "../logic/gesture.h"
#include "../logic/camera.h"
#include "../logic/combat.h"

#define TICK_MS 40
#define MAX_CATCHUP_MS 200   /* real ms simulated per callback at most: a stall must not teleport drones */
#define HUD_H 14
#define MARK_CONFIRM_PX 20   /* UI tuning: a tap this close to the marker uses the ability */
#define MARK_TTL_MS 3000     /* UI tuning: the marker disappears this long after its last placement */
#define BANNER_MS 2500       /* "NIGHT N" banner and raid arrows at the start of the night (sim time) */
#define BLIP_INSET 4         /* spawn blips sit this far inside the screen edge */
#define BLIP_BLINK_MS 150
#define ZONE_BLINK_MS 1000   /* an EW / Flare zone blinks during its last second */
#define AB_HUD_X 112         /* HUD: selected ability name (and cooldown seconds) */
#define AB_HUD_W 56
#define CARD_MS 2000         /* UI tuning: the ability card after Up/Down stays this long (real time) */

static Window *s_window;
static Layer *s_layer;
static AppTimer *s_timer;
static const Camera s_cam = {0, 0, 1};
static GestureTracker s_gt;
static TouchClock s_clock;
static uint32_t s_last_ms;
static bool s_fast, s_paused;
static Ability s_ability;
static bool s_mark;
static GPoint s_mark_pt;
static uint32_t s_mark_ms;
static bool s_card;          /* what the selected ability does, after Up/Down */
static uint32_t s_card_ms;

static const char *const AB_SHORT[AB_COUNT] = {"AIR", "EW", "FLARE"};
static const char *const AB_TITLE[AB_COUNT] = {"AIR STRIKE", "EW PULSE", "FLARE"};
static const char *const AB_DESC[AB_COUNT] = {
  "Destroys all drones in the circle after 1.5s",
  "Slows drones in the circle for 5s",
  "Lights the circle for 6s, towers in it get full range",
};

static uint32_t prv_now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return touch_clock_ms(&s_clock, (uint32_t)s, ms);
}


/* ---- drawing ---- */

static void prv_text(GContext *ctx, const char *txt, GRect r, GTextAlignment align, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, txt, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), r,
                     GTextOverflowModeTrailingEllipsis, align, NULL);
}

/* Line from (x0, y0) to (x1, y1) relative to c, scaled by k. */
static void prv_icon_line(GContext *ctx, GPoint c, int k, int x0, int y0, int x1, int y1) {
  graphics_draw_line(ctx, GPoint(c.x + x0 * k, c.y + y0 * k), GPoint(c.x + x1 * k, c.y + y1 * k));
}

/* Plane for Air Strike, lightning for EW Pulse, star for Flare; k scales it (1: 11 px). */
static void prv_ability_icon_k(GContext *ctx, Ability a, GPoint c, GColor col, int k) {
  graphics_context_set_stroke_color(ctx, col);
  graphics_context_set_stroke_width(ctx, k);
  switch (a) {
    case AB_AIR:
      prv_icon_line(ctx, c, k, 0, -5, 0, 4);     /* fuselage */
      prv_icon_line(ctx, c, k, -5, 0, 5, 0);     /* wings */
      prv_icon_line(ctx, c, k, -4, 1, 4, 1);
      prv_icon_line(ctx, c, k, -2, 4, 2, 4);     /* tail */
      break;
    case AB_EW:
      for (int dx = 0; dx < 2; dx++) {   /* two px thick zigzag */
        prv_icon_line(ctx, c, k, 2 + dx, -5, -2 + dx, 0);
        prv_icon_line(ctx, c, k, -2 + dx, 0, 2 + dx, 0);
        prv_icon_line(ctx, c, k, 2 + dx, 0, -2 + dx, 5);
      }
      break;
    default:   /* AB_FLARE */
      prv_icon_line(ctx, c, k, 0, -5, 0, 5);
      prv_icon_line(ctx, c, k, -5, 0, 5, 0);
      prv_icon_line(ctx, c, k, -3, -3, 3, 3);
      prv_icon_line(ctx, c, k, -3, 3, 3, -3);
      break;
  }
  graphics_context_set_stroke_width(ctx, 1);
}

static void prv_ability_icon(GContext *ctx, Ability a, GPoint c, GColor col) {
  prv_ability_icon_k(ctx, a, c, col, 1);
}

static unsigned prv_secs(uint16_t ms) { return (ms + 999) / 1000; }

/* Selected ability by name: yellow when it can be used now, gray otherwise; seconds left on cooldown. */
static void prv_draw_ability(GContext *ctx, const Game *g) {
  uint16_t cd = g->ab_cooldown_ms[s_ability];
  char buf[12];
  if (cd > 0) snprintf(buf, sizeof(buf), "%s %u", AB_SHORT[s_ability], prv_secs(cd));
  else snprintf(buf, sizeof(buf), "%s", AB_SHORT[s_ability]);
  prv_text(ctx, buf, GRect(AB_HUD_X, -3, AB_HUD_W, HUD_H + 4), GTextAlignmentCenter,
           ability_ready(g, s_ability) ? COL_SELECT : COL_DISABLED);
}

static void prv_draw_hud(GContext *ctx, const Game *g, int w) {
  /* No alpha on Pebble: black on every other row lets the map show through. */
  graphics_context_set_stroke_color(ctx, COL_HUD_BG);
  for (int y = 0; y < HUD_H; y += 2) graphics_draw_line(ctx, GPoint(0, y), GPoint(w - 1, y));
  char buf[16];
  snprintf(buf, sizeof(buf), "$%ld", (long)g->money);
  prv_text(ctx, buf, GRect(3, -3, 50, HUD_H + 4), GTextAlignmentLeft, COL_HUD_MONEY);
  snprintf(buf, sizeof(buf), "NIGHT %u", (unsigned)g->night);
  prv_text(ctx, buf, GRect(46, -3, 52, HUD_H + 4), GTextAlignmentLeft, COL_HUD_TEXT);
  if (s_fast) prv_text(ctx, "x2", GRect(97, -3, 16, HUD_H + 4), GTextAlignmentLeft, COL_SELECT);
  prv_draw_ability(ctx, g);
  int hp = plant_health_pct(g);
  snprintf(buf, sizeof(buf), "%d%%", hp);
  prv_text(ctx, buf, GRect(w - 34, -3, 31, HUD_H + 4), GTextAlignmentRight, health_color(hp));
}

static void prv_draw_mark(GContext *ctx, GPoint p, int radius, GColor col) {
  graphics_context_set_stroke_color(ctx, col);
  graphics_draw_circle(ctx, p, radius);
  graphics_draw_line(ctx, GPoint(p.x - 3, p.y), GPoint(p.x + 3, p.y));
  graphics_draw_line(ctx, GPoint(p.x, p.y - 3), GPoint(p.x, p.y + 3));
}

static GPoint prv_pt(Vec v) { return GPoint(v.x / FX, v.y / FX); }

/* Text with a 1 px black shadow: readable over the map. */
static void prv_shadow_text(GContext *ctx, const char *txt, GRect r, GColor col) {
  prv_text(ctx, txt, GRect(r.origin.x + 1, r.origin.y + 1, r.size.w, r.size.h), GTextAlignmentCenter, GColorBlack);
  prv_text(ctx, txt, r, GTextAlignmentCenter, col);
}

/* Under the mark (above it near the bottom edge): what a tap on it will do, or why it will not. */
static void prv_draw_mark_label(GContext *ctx, const Game *g, GRect b) {
  char buf[40];
  uint16_t cd = g->ab_cooldown_ms[s_ability];
  int32_t cost = ability_cost(g, s_ability);
  if (cd > 0) snprintf(buf, sizeof(buf), "Wait %us", prv_secs(cd));
  else if (g->money < cost) snprintf(buf, sizeof(buf), "Need $%ld", (long)cost);
  else snprintf(buf, sizeof(buf), "Tap again: %s $%ld", ABILITY_SPECS[s_ability].name, (long)cost);
  int radius = ABILITY_SPECS[s_ability].radius, w = 170, h = 18;
  int y = s_mark_pt.y + radius + 1;
  if (y + h > b.size.h) y = s_mark_pt.y - radius - h - 1;
  int x = s_mark_pt.x - w / 2;
  x = x < 0 ? 0 : x + w > b.size.w ? b.size.w - w : x;
  prv_shadow_text(ctx, buf, GRect(x, y, w, h), cd == 0 && g->money >= cost ? COL_HUD_TEXT : COL_DISABLED);
}

/* After Up/Down: big icon, name, what it does, price and recharge. */
static void prv_draw_card(GContext *ctx, const Game *g, GRect b) {
  GRect box = GRect(12, b.size.h / 2 - 46, b.size.w - 24, 92);
  graphics_context_set_fill_color(ctx, COL_HUD_BG);
  graphics_fill_rect(ctx, box, 4, GCornersAll);
  graphics_context_set_stroke_color(ctx, COL_SELECT);
  graphics_draw_round_rect(ctx, box, 4);
  prv_ability_icon_k(ctx, s_ability, GPoint(box.origin.x + 18, box.origin.y + 16), COL_SELECT, 2);
  graphics_context_set_text_color(ctx, COL_SELECT);
  graphics_draw_text(ctx, AB_TITLE[s_ability], fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(box.origin.x + 34, box.origin.y + 2, box.size.w - 38, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, COL_HUD_TEXT);
  graphics_draw_text(ctx, AB_DESC[s_ability], fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(box.origin.x + 6, box.origin.y + 32, box.size.w - 12, 34),
                     GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
  uint16_t cool = ability_cooldown(g, s_ability);
  char buf[24];
  if (cool % 1000) snprintf(buf, sizeof(buf), "$%ld \u00B7 recharge %u.%us", (long)ability_cost(g, s_ability),
                            (unsigned)(cool / 1000), (unsigned)(cool % 1000 / 100));
  else snprintf(buf, sizeof(buf), "$%ld \u00B7 recharge %us", (long)ability_cost(g, s_ability), (unsigned)(cool / 1000));
  prv_text(ctx, buf, GRect(box.origin.x + 6, box.origin.y + 68, box.size.w - 12, 18), GTextAlignmentLeft,
           COL_HUD_MONEY);
}

static bool prv_zone_shown(int16_t ms_left) {
  return ms_left > ZONE_BLINK_MS || (ms_left / BLIP_BLINK_MS) % 2 == 0;
}

/* Active EW Pulse (violet double ring) and Flare (pale yellow ring, white star in the middle). */
static void prv_draw_zones(GContext *ctx, const Game *g) {
  if (g->ew_ms > 0 && prv_zone_shown(g->ew_ms)) {
    GPoint p = prv_pt(g->ew_point);
    graphics_context_set_stroke_color(ctx, COL_EW_ZONE);
    graphics_draw_circle(ctx, p, ABILITY_SPECS[AB_EW].radius);
    graphics_draw_circle(ctx, p, ABILITY_SPECS[AB_EW].radius - 2);
  }
  if (g->flare_ms > 0 && prv_zone_shown(g->flare_ms)) {
    GPoint p = prv_pt(g->flare_point);
    graphics_context_set_stroke_color(ctx, COL_FLARE_ZONE);
    graphics_draw_circle(ctx, p, ABILITY_SPECS[AB_FLARE].radius);
    prv_ability_icon(ctx, AB_FLARE, p, GColorWhite);
  }
}

/* Kill streak under the HUD while it pays a multiplier. */
static void prv_draw_combo(GContext *ctx, const Game *g, int w) {
  if (g->combo < COMBO_X15_AT || g->combo_ms == 0) return;
  int pct = combo_pct(g->combo);
  char buf[24];
  snprintf(buf, sizeof(buf), "COMBO %u  x%s", (unsigned)g->combo, pct >= 200 ? "2" : "1.5");
  GRect r = GRect(0, HUD_H - 2, w, 18);
  prv_text(ctx, buf, GRect(r.origin.x + 1, r.origin.y + 1, r.size.w, r.size.h), GTextAlignmentCenter, GColorBlack);
  prv_text(ctx, buf, r, GTextAlignmentCenter, pct >= 200 ? GColorOrange : COL_SELECT);
}

/* A drone has just crossed the map edge: pulsing red blip there, so a raid that is still out of
 * sight is never silent. Clamped below the HUD. */
static void prv_draw_spawn_blips(GContext *ctx, const Game *g, GRect b) {
  for (int i = 0; i < MAX_EFFECTS; i++) {
    const Effect *e = &g->effects[i];
    if (!e->active || e->kind != FX_SPAWN) continue;
    int el = SPAWN_WARN_MS - (e->ttl_ms < 0 ? 0 : e->ttl_ms);
    int x = e->a.x / FX, y = e->a.y / FX;
    x = x < BLIP_INSET ? BLIP_INSET : x > b.size.w - 1 - BLIP_INSET ? b.size.w - 1 - BLIP_INSET : x;
    y = y < HUD_H + BLIP_INSET ? HUD_H + BLIP_INSET : y > b.size.h - 1 - BLIP_INSET ? b.size.h - 1 - BLIP_INSET : y;
    GPoint p = GPoint(x, y);
    graphics_context_set_stroke_color(ctx, GColorRed);
    graphics_draw_circle(ctx, p, 2 + el * 6 / SPAWN_WARN_MS);   /* expanding ping */
    if ((el / BLIP_BLINK_MS) % 2 == 0) {
      graphics_context_set_fill_color(ctx, GColorRed);
      graphics_fill_circle(ctx, p, 2);
    }
  }
}

/* Start of the night: which night it is and where the drones come from. */
static void prv_draw_banner(GContext *ctx, const Game *g, GRect b) {
  render_raid_arrows(ctx, g, GRect(0, HUD_H, b.size.w, b.size.h - HUD_H));
  GRect box = GRect(10, b.size.h / 3 - 24, b.size.w - 20, 64);
  graphics_context_set_fill_color(ctx, COL_HUD_BG);
  graphics_fill_rect(ctx, box, 4, GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_draw_round_rect(ctx, box, 4);
  char buf[16];
  snprintf(buf, sizeof(buf), "NIGHT %u", (unsigned)g->night);
  graphics_context_set_text_color(ctx, COL_HUD_TEXT);
  graphics_draw_text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(box.origin.x, box.origin.y, box.size.w, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  prv_text(ctx, "Drones incoming", GRect(box.origin.x, box.origin.y + 26, box.size.w, 18),
           GTextAlignmentCenter, GColorRed);
  graphics_context_set_text_color(ctx, COL_DISABLED);
  graphics_draw_text(ctx, "Up/Down: ability \u00B7 Tap map: aim", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(box.origin.x, box.origin.y + 43, box.size.w, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void prv_draw_pause(GContext *ctx, GRect b) {
  GRect box = GRect(10, b.size.h / 2 - 30, b.size.w - 20, 60);
  graphics_context_set_fill_color(ctx, COL_HUD_BG);
  graphics_fill_rect(ctx, box, 4, GCornersAll);
  graphics_context_set_stroke_color(ctx, COL_HUD_TEXT);
  graphics_draw_round_rect(ctx, box, 4);
  graphics_context_set_text_color(ctx, COL_HUD_TEXT);
  graphics_draw_text(ctx, "PAUSED", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                     GRect(box.origin.x, box.origin.y + 2, box.size.w, 28),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  prv_text(ctx, "Back: quit, Select: resume", GRect(box.origin.x, box.origin.y + 32, box.size.w, 20),
           GTextAlignmentCenter, COL_HUD_TEXT);
}

static void prv_update(Layer *layer, GContext *ctx) {
  const Game *g = app_game();
  GRect b = layer_get_bounds(layer);
  render_map(ctx, g, &s_cam, true);
  render_towers(ctx, g, &s_cam, false);
  prv_draw_zones(ctx, g);
  render_entities(ctx, g, &s_cam);
  if (g->air_active) prv_draw_mark(ctx, prv_pt(g->air_point), ABILITY_SPECS[AB_AIR].radius, COL_EXPLOSION);
  if (s_mark) {
    prv_draw_mark(ctx, s_mark_pt, ABILITY_SPECS[s_ability].radius,
                  ability_ready(g, s_ability) ? COL_AIR_MARK : COL_DISABLED);
    prv_draw_mark_label(ctx, g, b);
  }
  prv_draw_spawn_blips(ctx, g, b);
  prv_draw_combo(ctx, g, b.size.w);
  if (g->night_ms < BANNER_MS) prv_draw_banner(ctx, g, b);
  if (s_card && !s_paused) prv_draw_card(ctx, g, b);
  prv_draw_hud(ctx, g, b.size.w);
  if (s_paused) prv_draw_pause(ctx, b);
}

/* ---- simulation ---- */

static void prv_tick_cb(void *ctx);

static void prv_schedule(void) {
  if (s_layer && !s_paused && app_game()->phase == PHASE_NIGHT && !s_timer)
    s_timer = app_timer_register(TICK_MS, prv_tick_cb, NULL);
}

static void prv_finish_night(Game *g) {
  if (g->phase == PHASE_REPORT) {
    /* Income and night++ are already applied: the save loads as the next DAY, and the
       report's own save after report_ack writes the same state. */
    storage_save(g, false);
    app_go_report();
    return;
  }
  Best b = storage_best((Difficulty)g->difficulty);
  bool record = best_update(&b, nights_survived(g), g->score);
  if (record) storage_set_best((Difficulty)g->difficulty, b);
  storage_clear();
  gameover_window_set_record(record);
  app_go_gameover();
}

static void prv_tick_cb(void *ctx) {
  s_timer = NULL;
  Game *g = app_game();
  uint32_t now = prv_now_ms();
  uint32_t real = now - s_last_ms;
  s_last_ms = now;
  if (real > MAX_CATCHUP_MS) real = MAX_CATCHUP_MS;
  uint32_t sim = s_fast ? real * 2 : real;
  while (sim > 0 && g->phase == PHASE_NIGHT) {
    uint16_t step = sim > TICK_MS ? TICK_MS : (uint16_t)sim;
    sim_tick(g, step);
    sim -= step;
  }
  feedback_events(sim_take_events(g));
  if (g->phase == PHASE_REPORT || g->phase == PHASE_GAME_OVER) { prv_finish_night(g); return; }
  if (s_mark && now - s_mark_ms >= MARK_TTL_MS) s_mark = false;
  if (s_card && now - s_card_ms >= CARD_MS) s_card = false;
  layer_mark_dirty(s_layer);
  prv_schedule();
}

static void prv_set_paused(bool paused) {
  s_paused = paused;
  if (paused) {
    if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  } else {
    s_last_ms = prv_now_ms();
    prv_schedule();
  }
  layer_mark_dirty(s_layer);
}

static void prv_focus(bool in_focus) {
  /* a notification or system overlay: the night must not run unseen */
  if (!in_focus && !s_paused && app_game()->phase == PHASE_NIGHT) prv_set_paused(true);
}

/* ---- input ---- */

static void prv_tap(int16_t x, int16_t y) {
  Game *g = app_game();
  if (s_paused || g->phase != PHASE_NIGHT) return;
  if (s_mark && fx_dist(x - s_mark_pt.x, y - s_mark_pt.y) <= MARK_CONFIRM_PX) {
    Vec p = {s_mark_pt.x * FX, s_mark_pt.y * FX};
    if (ability_use(g, s_ability, p)) s_mark = false;
    else feedback_short();   /* on cooldown or no money: the mark stays, nothing is spent */
  } else {
    s_mark = true;
    s_mark_pt = GPoint(x, y);
    s_mark_ms = prv_now_ms();
  }
  layer_mark_dirty(s_layer);
}

static void prv_touch(const TouchEvent *e, void *ctx) {
  TouchKind k = e->type == TouchEvent_Touchdown ? TOUCH_DOWN
              : e->type == TouchEvent_Liftoff ? TOUCH_UP : TOUCH_MOVE;
  Gesture gs = gesture_feed(&s_gt, k, e->x, e->y, prv_now_ms());
  if (gs.kind == GES_TAP) prv_tap(gs.x, gs.y);
}

static void prv_select(ClickRecognizerRef r, void *ctx) {
  if (s_paused) { prv_set_paused(false); return; }
  s_fast = !s_fast;
  layer_mark_dirty(s_layer);
}

static void prv_cycle(int step) {
  if (s_paused) return;
  s_ability = (Ability)((s_ability + AB_COUNT + step) % AB_COUNT);
  s_card = true;
  s_card_ms = prv_now_ms();
  layer_mark_dirty(s_layer);
}

static void prv_up(ClickRecognizerRef r, void *ctx) { prv_cycle(-1); }
static void prv_down(ClickRecognizerRef r, void *ctx) { prv_cycle(1); }

static void prv_back(ClickRecognizerRef r, void *ctx) {
  if (s_paused) { app_go_menu(); return; }   /* the pending_night save from day start stays */
  prv_set_paused(true);
}

static void prv_clicks(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_back);
  window_single_click_subscribe(BUTTON_ID_UP, prv_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_down);
}

/* ---- window ---- */

static void prv_load(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, prv_update);
  layer_add_child(root, s_layer);
  s_fast = false;
  s_paused = false;
  s_mark = false;
  s_card = false;
  s_ability = AB_AIR;
  s_clock = (TouchClock){0};
  s_last_ms = prv_now_ms();
  prv_schedule();
  app_focus_service_subscribe_handlers((AppFocusHandlers){ .will_focus = prv_focus });
}

static void prv_unload(Window *w) {
  app_focus_service_unsubscribe();
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  layer_destroy(s_layer);
  s_layer = NULL;
}

static void prv_appear(Window *w) {
  s_gt = (GestureTracker){0};
  touch_service_subscribe(prv_touch, NULL);
}

static void prv_disappear(Window *w) {
  touch_service_unsubscribe();
}

void night_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, COL_NIGHT_BG);
    window_set_touch_bridge_disabled(s_window, true);
    window_set_click_config_provider(s_window, prv_clicks);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = prv_load, .appear = prv_appear, .disappear = prv_disappear, .unload = prv_unload });
  }
  window_stack_push(s_window, false);
}

void night_window_destroy(void) {
  if (s_window) { window_destroy(s_window); s_window = NULL; }
}
