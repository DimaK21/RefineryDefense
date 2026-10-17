#include <stdlib.h>
#include "test.h"
#include "config.h"
#include "economy.h"
#include "sim.h"

#define SEEDS 20
#define MAX_NIGHTS 20   /* lowered from 60 to keep the run to a few seconds */
#define TICKS_CAP 5000

static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

static int has_radar(const Game *g) {
  for (int i = 0; i < MAX_TOWERS; i++)
    if (g->towers[i].active && g->towers[i].type == TW_RADAR) return 1;
  return 0;
}

static int d2(int c, int r, int cc, int cr) { return (c - cc) * (c - cc) + (r - cr) * (r - cr); }

/* Free cell nearest to (cc,cr); returns 0 if none. */
static int nearest_free(const Game *g, int cc, int cr, int *oc, int *or_) {
  int best = 1 << 30; *oc = *or_ = 0;
  for (int r = 0; r < GRID_ROWS; r++)
    for (int c = 0; c < GRID_COLS; c++)
      if (cell_free(g, c, r) && d2(c, r, cc, cr) < best) { best = d2(c, r, cc, cr); *oc = c; *or_ = r; }
  return best < (1 << 30);
}

/* Free cell in ring radius 2..4 (Chebyshev, in cells) around the 2x2 object, nearest to its centre. */
static int ring_free(const Game *g, ObjectId o, int *oc, int *or_) {
  int c0 = OBJECT_SPECS[o].col, r0 = OBJECT_SPECS[o].row;
  int best = 1 << 30;
  for (int r = r0 - 4; r <= r0 + 5; r++)
    for (int c = c0 - 4; c <= c0 + 5; c++) {
      if (c < 0 || r < 0 || c >= GRID_COLS || r >= GRID_ROWS || !cell_free(g, c, r)) continue;
      int dx = c < c0 ? c0 - c : (c > c0 + 1 ? c - c0 - 1 : 0);
      int dy = r < r0 ? r0 - r : (r > r0 + 1 ? r - r0 - 1 : 0);
      int dist = dx > dy ? dx : dy;
      if (dist < 2 || dist > 4) continue;
      int d = (2 * c + 1 - 2 * c0 - 2) * (2 * c + 1 - 2 * c0 - 2) + (2 * r + 1 - 2 * r0 - 2) * (2 * r + 1 - 2 * r0 - 2);
      if (d < best) { best = d; *oc = c; *or_ = r; }
    }
  return best < (1 << 30);
}

static void bot_morning(Game *g) {
  for (int o = 0; o < OBJ_COUNT; o++)
    if (g->objects[o].status == OBJ_ALIVE && g->objects[o].hp < 60) object_action(g, (ObjectId)o);
  for (int i = 0; i < MAX_TOWERS; i++)
    if (g->towers[i].active && (g->towers[i].type == TW_TOR || g->towers[i].type == TW_PATRIOT))
      tower_reload(g, i);
  int c, r;
  if (!has_radar(g) && nearest_free(g, 8, 9, &c, &r)) tower_buy(g, TW_RADAR, c, r);
  int turn = 0, obj = 0, fails = 0;
  while (fails < 2 * OBJ_COUNT) {                 /* until money runs out / no room */
    while (g->objects[obj % OBJ_COUNT].status != OBJ_ALIVE) obj++;
    ObjectId o = (ObjectId)(obj % OBJ_COUNT); obj++;
    TowerType t = (turn & 1) ? TW_TOR : TW_GEPARD;
    if (ring_free(g, o, &c, &r) && tower_buy(g, t, c, r) == ACT_OK) { turn++; fails = 0; }
    else fails++;
    if (g->money < TOWER_SPECS[TW_GEPARD].cost) break;
    if (alive_objects(g) == 0) break;
  }
}

/* Returns nights survived (capped at MAX_NIGHTS); *lost_night = g.night at game over, 0 if none. */
static int run(uint32_t seed, int smart, int *lost_night) {
  Game g; game_new(&g, seed);
  *lost_night = 0;
  while (g.night <= MAX_NIGHTS) {
    if (smart) bot_morning(&g);
    night_begin(&g);
    for (int i = 0; i < TICKS_CAP && g.phase == PHASE_NIGHT; i++) sim_tick(&g, 40);
    if (g.phase == PHASE_GAME_OVER) { *lost_night = g.night; return nights_survived(&g); }
    if (g.phase != PHASE_REPORT) break;
    report_ack(&g);
  }
  return nights_survived(&g);
}

static void test_balance(void) {
  int smart[SEEDS], idle[SEEDS], lost;
  int idle_ok = 1;
  for (int s = 0; s < SEEDS; s++) {
    smart[s] = run((uint32_t)(s + 1), 1, &lost);
    idle[s] = run((uint32_t)(s + 1), 0, &lost);
    if (!(lost >= 1 && lost <= 3)) idle_ok = 0;
    printf("seed %2d: bot %2d nights, idle %2d nights (idle lost night %d)\n", s + 1, smart[s], idle[s], lost);
  }
  int sorted[SEEDS]; for (int s = 0; s < SEEDS; s++) sorted[s] = smart[s];
  qsort(sorted, SEEDS, sizeof(int), cmp_int);
  int median = sorted[SEEDS / 2];                  /* upper median: sorted[10] */
  printf("bot median (sorted[10]) = %d, min %d, max %d\n", median, sorted[0], sorted[SEEDS - 1]);
  CHECK(median >= 5);
  CHECK(median < MAX_NIGHTS);
  CHECK(idle_ok);
}

int main(void) {
  RUN(test_balance);
  return TEST_SUMMARY();
}
