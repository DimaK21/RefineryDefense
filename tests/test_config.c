#include "test.h"
#include "config.h"
#include "economy.h"

static void test_specs(void) {
  CHECK_EQ(TOWER_SPECS[TW_PATRIOT].cost, 250);
  CHECK_EQ(TOWER_SPECS[TW_PATRIOT].ammo, 3);
  CHECK_EQ(DRONE_SPECS[DR_SHAHED238].first_night, 6);
  CHECK_EQ(DRONE_SPECS[DR_GERBERA].damage, 0);
  CHECK_EQ(DRONE_SPECS[DR_HARPY].damage, 10);
  CHECK_EQ(OBJECT_SPECS[OBJ_CRACKER].income, 40);
}

static void test_object_layout(void) {
  for (int i = 0; i < OBJ_COUNT; i++) {
    const ObjectSpec *a = &OBJECT_SPECS[i];
    CHECK(a->col + 1 < GRID_COLS && a->row + 1 < GRID_ROWS);
    for (int j = i + 1; j < OBJ_COUNT; j++) {
      const ObjectSpec *b = &OBJECT_SPECS[j];
      bool overlap = a->col < b->col + 2 && b->col < a->col + 2 &&
                     a->row < b->row + 2 && b->row < a->row + 2;
      CHECK(!overlap);
    }
  }
}

static void test_game_new(void) {
  Game g;
  game_new(&g, 1, DIFF_NORMAL);
  CHECK_EQ(g.phase, PHASE_DAY);
  CHECK_EQ(g.night, 1);
  CHECK_EQ(g.money, DIFFICULTY_SPECS[DIFF_NORMAL].start_money);
  CHECK_EQ(g.objects[OBJ_DISTILLATION].status, OBJ_ALIVE);
  CHECK_EQ(g.objects[OBJ_DISTILLATION].hp, 100);
  CHECK_EQ(g.objects[OBJ_TANK_A].status, OBJ_ALIVE);
  CHECK_EQ(g.objects[OBJ_CRACKER].status, OBJ_NOT_BUILT);
  CHECK_EQ(alive_objects(&g), 2);
}

static void test_difficulty(void) {
  Game g;
  for (int d = 0; d < DIFF_COUNT; d++) {
    game_new(&g, 1, (Difficulty)d);
    CHECK_EQ(g.difficulty, d);
    CHECK_EQ(g.money, DIFFICULTY_SPECS[d].start_money);
  }
  CHECK(DIFFICULTY_SPECS[DIFF_EASY].start_money > DIFFICULTY_SPECS[DIFF_NORMAL].start_money);
  CHECK(DIFFICULTY_SPECS[DIFF_HARD].wave_pct > DIFFICULTY_SPECS[DIFF_NORMAL].wave_pct);
  CHECK(DIFFICULTY_SPECS[DIFF_EASY].wave_pct < DIFFICULTY_SPECS[DIFF_NORMAL].wave_pct);
  CHECK_EQ(DIFFICULTY_SPECS[DIFF_NORMAL].wave_pct, 100);
  game_new(&g, 1, DIFF_COUNT);                       /* out of range falls back to Normal */
  CHECK_EQ(g.difficulty, DIFF_NORMAL);
}

int main(void) {
  RUN(test_specs);
  RUN(test_object_layout);
  RUN(test_game_new);
  RUN(test_difficulty);
  return TEST_SUMMARY();
}
