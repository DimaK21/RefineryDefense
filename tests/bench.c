/* Host microbenchmark (not part of `make test`): night CPU with 30 towers and 64 drones. */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "config.h"
#include "economy.h"
#include "drones.h"
#include "combat.h"
#include "sim.h"
#include "fx.h"

#define ITERS 20000

static Game s_base, s_work;   /* never on the stack */
static volatile int s_sink;

static double now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1e9 + ts.tv_nsec;
}

static void setup(void) {
  static const TowerType mix[] = {TW_MG, TW_GEPARD, TW_TOR, TW_PATRIOT, TW_MG, TW_GEPARD, TW_JAMMER, TW_RADAR};
  game_new_start(&s_base, 1, DIFF_NORMAL, OBJ_TANK_A);
  s_base.money = 1000000;
  int n = 0, k = 0;
  for (int r = 1; r < GRID_ROWS - 1 && n < 30; r += 2)
    for (int c = 1; c < GRID_COLS - 1 && n < 30; c += 2)
      if (tower_buy(&s_base, mix[k % 8], c, r) == ACT_OK) { n++; k++; }
  night_begin(&s_base);
  s_base.wave_len = 0;   /* no spawning: fixed population */
  for (int i = 0; i < MAX_DRONES; i++) {
    Drone *d = &s_base.drones[i];
    int type = i % DR_COUNT;
    d->active = 1; d->type = (uint8_t)type; d->hp = 255; d->uid = (uint16_t)(i + 1);
    d->target_kind = TGT_OBJECT; d->target_idx = OBJ_DISTILLATION;
    d->pos.x = d->track.x = (10 + (i * 37) % 180) * FX;
    d->pos.y = d->track.y = (10 + (i * 53) % 208) * FX;
    d->wps[0] = d->wps[1] = d->wps[2] = object_center(OBJ_DISTILLATION);
  }
  printf("towers %d, drones %d\n", n, MAX_DRONES);
}

/* What render_entities does per frame for visibility. */
static void frame(const Game *g) {
  static int32_t ranges[MAX_TOWERS];
  int vis = 0;
  combat_engage_ranges(g, ranges);
  for (int d = 0; d < MAX_DRONES; d++) if (g->drones[d].active && drone_visible_in(g, d, ranges)) vis++;
  s_sink += vis;
}

static double run(int ticks, int with_frame) {
  double t0 = now_ns();
  for (int i = 0; i < ITERS; i++) {
    memcpy(&s_work, &s_base, sizeof(Game));
    for (int k = 0; k < ticks; k++) sim_tick(&s_work, 40);
    if (with_frame) frame(&s_work);
  }
  return (now_ns() - t0) / ITERS;
}

int main(void) {
  setup();
  double copy = run(0, 0);
  double tick = run(1, 0) - copy;
  double vis = run(0, 1) - copy;
  double x2 = run(2, 1) - copy;
  printf("sim_tick (all towers ready):  %8.0f ns\n", tick);
  printf("render visibility (64 drones): %8.0f ns\n", vis);
  printf("x2 callback (2 ticks + frame): %8.0f ns\n", x2);
  return s_sink == -1;
}
