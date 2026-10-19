#include <stdlib.h>
#include "test.h"
#include "config.h"
#include "economy.h"
#include "sim.h"
#include "combat.h"
#include "fx.h"

/* Balance criteria (bot = bot_morning + bot_night below, seeds 1..SEEDS):
 * The bot's outcome is bimodal: a seed either loses around nights 4-8 (an early leak costs repairs and
 * missile reloads, the defence stops growing, Harpies take out the radar, then Shahed-238 from four
 * sides on night 6 finishes it) or snowballs to the wall at nights 16-21, where the bot runs out of
 * ring cells and Shahed-238 waves outgrow a fixed defence. A median of 20 seeds sat in the gap
 * between the modes and flipped on tiny changes, so the checks below look at robust statistics:
 *   - mean nights in [BAL_MEAN_MIN, BAL_MEAN_MAX]: overall difficulty (the upper bound is the wall);
 *   - lower quartile >= BAL_Q1_MIN: at least 3/4 of seeds survive past the night-6 step;
 *   - at most BAL_EARLY_MAX seeds lost on night BAL_EARLY_NIGHT or earlier: early nights must not
 *     hinge on the seed (raid side vs. where the bot happens to put its towers);
 *   - no seed reaches MAX_NIGHTS, and an idle player (no towers, no abilities) loses on nights 1-3.
 * Reference with DIFFICULTY_SPECS[DIFF_NORMAL].start_money 500 over 1000 seeds: mean 15.6, Q1 14, median 18, 1.2% lost by night 5.
 * At DIFFICULTY_SPECS[DIFF_NORMAL].start_money 400 it was mean 8.3, Q1 5, median 6, 12.8% lost by night 5.
 * Plant roles with a random second unit, 60 seeds: Normal mean 16.3, Q1 12, median 18, 2 lost by night 5;
 * by start unit Tank Farm A 16.3, Tank Farm B 16.0, Loading Terminal 17.0, Cracking Unit 17.7.
 * Air Strike landing at once (was 1.5 s later): mean 17.1, Q1 17, median 19, 2 lost by night 5;
 * by start unit 16.4-18.3. */
#define SEEDS 60
#define BAL_MEAN_MIN 11
#define BAL_MEAN_MAX 20
#define BAL_Q1_MIN 6
#define BAL_EARLY_NIGHT 5
#define BAL_EARLY_MAX (SEEDS / 20)   /* 5% */
#define BAL_START_GAP 3       /* mean nights per second plant unit at most this far from Normal's */
#define BAL_DIFF_GAP 3        /* Easy / Hard mean nights at least this far from Normal */
#define BAL_EASY_IDLE_MAX 5   /* an idle player on Easy loses by this night */
#define MAX_NIGHTS 60
#define TICKS_CAP 5000
#define BOT_REACT_TICKS 10   /* the night bot looks at the sky every 400 ms, like a player */
#define BOT_CLUSTER 3        /* fire an ability at this many visible drones or more */

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
  reload_all(g);
  int c, r;
  if (!has_radar(g) && nearest_free(g, 8, 9, &c, &r)) tower_buy(g, TW_RADAR, c, r);
  int turn = 0, obj = 0, fails = 0;
  while (fails < 2 * OBJ_COUNT && alive_objects(g) > 0) {   /* until money runs out / no room */
    while (g->objects[obj % OBJ_COUNT].status != OBJ_ALIVE) obj++;
    ObjectId o = (ObjectId)(obj % OBJ_COUNT); obj++;
    TowerType t = (turn & 1) ? TW_TOR : TW_GEPARD;
    if (t == TW_TOR && g->money < tower_price(g, TW_TOR)) t = TW_GEPARD;
    if (ring_free(g, o, &c, &r) && tower_buy(g, t, c, r) == ACT_OK) { turn++; fails = 0; }
    else fails++;
    if (g->money < tower_price(g, TW_GEPARD)) break;
  }
}

/* Night: Air Strike, then EW Pulse at the densest cluster of visible drones. */
static void bot_night(Game *g) {
  static const Ability use[2] = {AB_AIR, AB_EW};
  for (int u = 0; u < 2; u++) {
    Ability a = use[u];
    if (!ability_ready(g, a)) continue;
    int32_t r = ABILITY_SPECS[a].radius * FX;
    int best = -1, best_n = 0;
    for (int i = 0; i < MAX_DRONES; i++) {
      if (!g->drones[i].active || !drone_visible(g, i)) continue;
      int n = 0;
      for (int j = 0; j < MAX_DRONES; j++)
        if (g->drones[j].active && drone_visible(g, j) &&
            fx_within(g->drones[j].pos.x - g->drones[i].pos.x, g->drones[j].pos.y - g->drones[i].pos.y, r)) n++;
      if (n > best_n) { best_n = n; best = i; }
    }
    if (best >= 0 && best_n >= BOT_CLUSTER) ability_use(g, a, g->drones[best].pos);
  }
}

/* Returns nights survived (capped at MAX_NIGHTS); *lost_night = g.night at game over, 0 if none.
 * start < 0: random second plant unit, as in a real game; otherwise that unit. */
static int run_start(uint32_t seed, int smart, Difficulty d, int start, int *lost_night) {
  Game g;
  if (start < 0) game_new(&g, seed, d); else game_new_start(&g, seed, d, (ObjectId)start);
  *lost_night = 0;
  while (g.night <= MAX_NIGHTS) {
    if (smart) bot_morning(&g);
    night_begin(&g);
    for (int i = 0; i < TICKS_CAP && g.phase == PHASE_NIGHT; i++) {
      if (smart && i % BOT_REACT_TICKS == 0) bot_night(&g);
      sim_tick(&g, 40);
    }
    CHECK(g.phase != PHASE_NIGHT);                 /* a night must end within the tick cap */
    if (g.phase == PHASE_GAME_OVER) { *lost_night = g.night; return nights_survived(&g); }
    if (g.phase != PHASE_REPORT) break;
    report_ack(&g);
  }
  return nights_survived(&g);
}

static int run(uint32_t seed, int smart, Difficulty d, int *lost_night) {
  return run_start(seed, smart, d, -1, lost_night);
}

typedef struct { int mean_x100, q1, early, capped, idle_best, idle_worst; } BalStats;

static BalStats measure(Difficulty d) {
  int smart[SEEDS], lost;
  BalStats st = {0, 0, 0, 0, 1 << 30, 0};
  int sum = 0;
  for (int s = 0; s < SEEDS; s++) {
    smart[s] = run((uint32_t)(s + 1), 1, d, &lost);
    if (lost && lost <= BAL_EARLY_NIGHT) st.early++;
    st.capped += smart[s] >= MAX_NIGHTS;
    sum += smart[s];
    run((uint32_t)(s + 1), 0, d, &lost);
    if (!lost) lost = MAX_NIGHTS + 1;
    if (lost < st.idle_best) st.idle_best = lost;
    if (lost > st.idle_worst) st.idle_worst = lost;
  }
  printf("%s: bot nights by seed:", DIFFICULTY_SPECS[d].name);
  for (int s = 0; s < SEEDS; s++) printf("%s%2d", s % 20 ? " " : "\n  ", smart[s]);
  int sorted[SEEDS]; for (int s = 0; s < SEEDS; s++) sorted[s] = smart[s];
  qsort(sorted, SEEDS, sizeof(int), cmp_int);
  st.q1 = sorted[SEEDS / 4];
  st.mean_x100 = sum * 100 / SEEDS;
  printf("\n  bot mean %.2f, min %d, Q1 %d, median %d, Q3 %d, max %d\n", (double)sum / SEEDS, sorted[0], st.q1,
         sorted[SEEDS / 2], sorted[3 * SEEDS / 4], sorted[SEEDS - 1]);
  printf("  lost by night %d: %d of %d; reached the %d-night cap: %d; idle lost on nights %d-%d\n",
         BAL_EARLY_NIGHT, st.early, SEEDS, MAX_NIGHTS, st.capped, st.idle_best, st.idle_worst);
  return st;
}

static BalStats s_normal;

static void test_balance(void) {
  BalStats n = s_normal = measure(DIFF_NORMAL);
  CHECK(n.mean_x100 >= BAL_MEAN_MIN * 100);
  CHECK(n.mean_x100 <= BAL_MEAN_MAX * 100);
  CHECK(n.q1 >= BAL_Q1_MIN);
  CHECK(n.early <= BAL_EARLY_MAX);
  CHECK(n.capped == 0);
  CHECK(n.idle_best >= 1 && n.idle_worst <= 3);
}

/* Easy and Hard must stay ordered around Normal by a clear margin, and still end. Reference over 60 seeds:
 * Easy ($600, waves 70%) mean 23.4, Q1 22, 0 lost by night 5; Hard ($500, waves 160%) mean 10.1, Q1 7,
 * median 9, 1 lost by night 5 (instant Air Strike; before plant roles: Easy 22.5, Hard 10.5). Hard with $450 and waves 140%
 * lost 12 of 60 by night 5. */
static void test_difficulties(void) {
  BalStats n = s_normal, e = measure(DIFF_EASY), h = measure(DIFF_HARD);
  CHECK(e.mean_x100 >= n.mean_x100 + BAL_DIFF_GAP * 100);
  CHECK(h.mean_x100 <= n.mean_x100 - BAL_DIFF_GAP * 100);
  CHECK(e.early <= BAL_EARLY_MAX);
  CHECK(e.capped == 0);
  CHECK(e.idle_worst <= BAL_EASY_IDLE_MAX);            /* even Easy needs towers */
  CHECK(h.early <= 2 * BAL_EARLY_MAX);
  CHECK(h.idle_worst <= 3);
}

/* Every second plant unit (they bring different roles) is a fair start on Normal. */
static void test_start_units(void) {
  printf("Normal: bot mean by start unit:");
  for (int o = OBJ_TANK_A; o < OBJ_COUNT; o++) {
    int sum = 0, lost;
    for (int s = 0; s < SEEDS; s++) sum += run_start((uint32_t)(s + 1), 1, DIFF_NORMAL, o, &lost);
    int mean_x100 = sum * 100 / SEEDS;
    printf(" %s %.2f", OBJECT_SPECS[o].name, (double)sum / SEEDS);
    CHECK(mean_x100 >= s_normal.mean_x100 - BAL_START_GAP * 100);
    CHECK(mean_x100 <= s_normal.mean_x100 + BAL_START_GAP * 100);
  }
  printf("\n");
}

int main(void) {
  RUN(test_balance);
  RUN(test_start_units);
  RUN(test_difficulties);
  return TEST_SUMMARY();
}
