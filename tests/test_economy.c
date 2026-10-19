#include "test.h"
#include "config.h"
#include "economy.h"

static void test_day_economy(void) {
  Game g; game_new_start(&g, 1, DIFF_NORMAL, OBJ_TANK_A);
  CHECK_EQ(tower_buy(&g, TW_MG, 0, 0), ACT_OK); CHECK_EQ(g.money, DIFFICULTY_SPECS[DIFF_NORMAL].start_money - 30);
  CHECK_EQ(tower_buy(&g, TW_MG, 0, 0), ACT_BLOCKED);
  CHECK_EQ(tower_buy(&g, TW_MG, 7, 8), ACT_BLOCKED);           /* Distillation cell */
  CHECK_EQ(tower_buy(&g, TW_MG, 16, 0), ACT_BLOCKED);          /* outside grid */
  int32_t keep = g.money; g.money = 120;                       /* too poor for a Patriot */
  CHECK_EQ(tower_buy(&g, TW_PATRIOT, 1, 1), ACT_NO_MONEY); CHECK_EQ(g.money, 120); g.money = keep;
  int t = tower_at(&g, 0, 0);
  CHECK_EQ(tower_move(&g, t, 2, 2), ACT_OK); CHECK_EQ(tower_at(&g, 2, 2), t); CHECK_EQ(g.money, DIFFICULTY_SPECS[DIFF_NORMAL].start_money - 30);
  g.towers[t].hp = 1;                                          /* MG: hp 3, cost 30 */
  CHECK_EQ(tower_repair_cost(&g, t), 6);                       /* ceil(9 * 2/3) */
  CHECK_EQ(tower_sell_value(&g, t), 15); CHECK_EQ(tower_sell_value(&g, MAX_TOWERS), 0);
  CHECK_EQ(tower_sell(&g, t), ACT_OK); CHECK_EQ(g.money, DIFFICULTY_SPECS[DIFF_NORMAL].start_money - 15); CHECK_EQ(tower_at(&g, 2, 2), -1);
  CHECK_EQ(tower_sell_value(&g, t), 0);
  g.money = 1000; tower_buy(&g, TW_TOR, 5, 5); int tor = tower_at(&g, 5, 5);
  CHECK_EQ(tower_reload_cost(&g, tor), 0); CHECK_EQ(tower_reload(&g, tor), ACT_INVALID);
  g.towers[tor].ammo = 2; CHECK_EQ(tower_reload_cost(&g, tor), 20);
  g.objects[OBJ_TANK_A].hp = 40; CHECK_EQ(object_action_cost(&g, OBJ_TANK_A), 72);
  g.objects[OBJ_TANK_B].status = OBJ_DESTROYED; CHECK_EQ(object_action_cost(&g, OBJ_TANK_B), 180);
  CHECK_EQ(object_action_cost(&g, OBJ_CRACKER), 250);
  CHECK_EQ(morning_income(&g), 30 + 8);                        /* Distillation 100% + Tank A 40% */
  g.money = 10; CHECK_EQ(object_action(&g, OBJ_CRACKER), ACT_NO_MONEY); CHECK_EQ(g.money, 10);
  g.phase = PHASE_NIGHT; CHECK_EQ(tower_buy(&g, TW_MG, 9, 0), ACT_INVALID);
}

static void test_plant_health(void) {
  static Game g; game_new_start(&g, 1, DIFF_NORMAL, OBJ_TANK_A);
  CHECK_EQ(plant_health_pct(&g), 100);                         /* two built, both full */
  g.objects[OBJ_TANK_A].hp = 40; CHECK_EQ(plant_health_pct(&g), 70);
  g.objects[OBJ_TANK_A].status = OBJ_DESTROYED; g.objects[OBJ_TANK_A].hp = 0;
  CHECK_EQ(plant_health_pct(&g), 50);                          /* destroyed counts as 0 */
  g.objects[OBJ_TANK_A].hp = 70; CHECK_EQ(plant_health_pct(&g), 50);  /* hp ignored unless alive */
  for (int o = 0; o < OBJ_COUNT; o++) g.objects[o].status = OBJ_NOT_BUILT;
  CHECK_EQ(plant_health_pct(&g), 0);
}

static void test_start_object(void) {
  int seen[OBJ_COUNT] = {0};
  for (uint32_t seed = 1; seed <= 200; seed++) {
    static Game g, h;
    game_new(&g, seed, DIFF_NORMAL);
    CHECK(object_alive(&g, OBJ_DISTILLATION));
    int n = 0;
    for (int o = OBJ_TANK_A; o < OBJ_COUNT; o++)
      if (object_alive(&g, (ObjectId)o)) { n++; seen[o]++; }
    CHECK_EQ(n, 1);
    game_new_start(&h, seed, DIFF_NORMAL, OBJ_TANK_A);
    CHECK_EQ(g.seed, h.seed);                                  /* the start pick does not move the night seed */
  }
  for (int o = OBJ_TANK_A; o < OBJ_COUNT; o++) CHECK(seen[o] > 0);
}

static void test_discounted(void) {
  CHECK_EQ(discounted(5, 50), 2); CHECK_EQ(discounted(1, 50), 1);
  CHECK_EQ(discounted(0, 50), 0); CHECK_EQ(discounted(70, 15), 59);
}

static void test_role_discounts(void) {
  static Game g; game_new_start(&g, 1, DIFF_NORMAL, OBJ_TANK_A); g.money = 1000;
  CHECK_EQ(tower_price(&g, TW_MG), 30);                        /* no Terminal: base prices */
  CHECK_EQ(missile_price(&g, TW_TOR), 5); CHECK_EQ(missile_price(&g, TW_PATRIOT), 30);
  g.objects[OBJ_TERMINAL].status = OBJ_ALIVE; g.objects[OBJ_TERMINAL].hp = 10;   /* any HP counts */
  CHECK_EQ(tower_price(&g, TW_MG), 25); CHECK_EQ(tower_price(&g, TW_PATRIOT), 212);
  CHECK_EQ(tower_buy(&g, TW_GEPARD, 0, 0), ACT_OK); CHECK_EQ(g.money, 1000 - 59);
  CHECK_EQ(tower_buy(&g, TW_MG, 1, 0), ACT_OK); int mg = tower_at(&g, 1, 0);
  g.towers[mg].hp = 1; CHECK_EQ(tower_repair_cost(&g, mg), 5);  /* discounted(6, 15) */
  CHECK_EQ(tower_sell_value(&g, mg), 15);                       /* refund from the base price */
  g.objects[OBJ_TANK_B].status = OBJ_ALIVE; g.objects[OBJ_TANK_B].hp = 100;
  CHECK_EQ(missile_price(&g, TW_TOR), 2); CHECK_EQ(missile_price(&g, TW_PATRIOT), 15);
  CHECK_EQ(tower_buy(&g, TW_TOR, 2, 0), ACT_OK); int tor = tower_at(&g, 2, 0);
  g.towers[tor].ammo = 2; CHECK_EQ(tower_reload_cost(&g, tor), 8);
  g.objects[OBJ_TANK_B].status = OBJ_DESTROYED; g.objects[OBJ_TERMINAL].status = OBJ_DESTROYED;
  CHECK_EQ(missile_price(&g, TW_TOR), 5); CHECK_EQ(tower_reload_cost(&g, tor), 20);
  CHECK_EQ(tower_price(&g, TW_MG), 30); CHECK_EQ(tower_repair_cost(&g, mg), 6);
}

static void test_reload_all(void) {
  static Game g; game_new_start(&g, 1, DIFF_NORMAL, OBJ_TANK_A); g.money = 1000;
  CHECK_EQ(reload_all(&g), ACT_INVALID);                       /* nothing to reload */
  tower_buy(&g, TW_TOR, 0, 0); tower_buy(&g, TW_TOR, 1, 0); tower_buy(&g, TW_PATRIOT, 2, 0);
  int a = tower_at(&g, 0, 0), b = tower_at(&g, 1, 0), p = tower_at(&g, 2, 0);
  CHECK_EQ(reload_all_cost(&g), 0); CHECK_EQ(reload_all(&g), ACT_INVALID);
  g.towers[a].ammo = 0; g.towers[b].ammo = 4;
  CHECK_EQ(reload_all_cost(&g), 40);
  g.money = 1000; CHECK_EQ(reload_all(&g), ACT_OK);
  CHECK_EQ(g.money, 960); CHECK_EQ(g.towers[a].ammo, 6); CHECK_EQ(g.towers[b].ammo, 6); CHECK_EQ(g.towers[p].ammo, 3);
  g.towers[a].ammo = 0; g.towers[b].ammo = 4;
  g.money = 15; CHECK_EQ(reload_all(&g), ACT_OK);               /* partial: emptiest first */
  CHECK_EQ(g.towers[a].ammo, 3); CHECK_EQ(g.towers[b].ammo, 4); CHECK_EQ(g.money, 0);
  g.towers[a].ammo = 0; g.towers[b].ammo = 0;
  g.money = 10; CHECK_EQ(reload_all(&g), ACT_OK);               /* one each, nobody left empty */
  CHECK_EQ(g.towers[a].ammo, 1); CHECK_EQ(g.towers[b].ammo, 1); CHECK_EQ(g.money, 0);
  g.money = 4; CHECK_EQ(reload_all(&g), ACT_NO_MONEY);
  CHECK_EQ(g.money, 4); CHECK_EQ(g.towers[a].ammo, 1);
  g.towers[a].ammo = 6; g.towers[b].ammo = 5; g.towers[p].ammo = 0;
  g.money = 20; CHECK_EQ(reload_all(&g), ACT_OK);               /* Patriot unaffordable: the Tor still gets one */
  CHECK_EQ(g.towers[b].ammo, 6); CHECK_EQ(g.towers[p].ammo, 0); CHECK_EQ(g.money, 15);
  g.money = 1000; g.phase = PHASE_NIGHT; CHECK_EQ(reload_all(&g), ACT_INVALID); CHECK_EQ(g.money, 1000);
}

int main(void) {
  RUN(test_role_discounts);
  RUN(test_reload_all);
  RUN(test_start_object);
  RUN(test_discounted);
  RUN(test_day_economy);
  RUN(test_plant_health);
  return TEST_SUMMARY();
}
