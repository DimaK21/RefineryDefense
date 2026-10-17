#include "test.h"
#include "config.h"
#include "economy.h"

static void test_day_economy(void) {
  Game g; game_new(&g, 1);
  CHECK_EQ(tower_buy(&g, TW_MG, 0, 0), ACT_OK); CHECK_EQ(g.money, 120);
  CHECK_EQ(tower_buy(&g, TW_MG, 0, 0), ACT_BLOCKED);
  CHECK_EQ(tower_buy(&g, TW_MG, 7, 8), ACT_BLOCKED);           /* Distillation cell */
  CHECK_EQ(tower_buy(&g, TW_MG, 16, 0), ACT_BLOCKED);          /* outside grid */
  CHECK_EQ(tower_buy(&g, TW_PATRIOT, 1, 1), ACT_NO_MONEY); CHECK_EQ(g.money, 120);
  int t = tower_at(&g, 0, 0);
  CHECK_EQ(tower_move(&g, t, 2, 2), ACT_OK); CHECK_EQ(tower_at(&g, 2, 2), t); CHECK_EQ(g.money, 120);
  g.towers[t].hp = 1;                                          /* MG: hp 3, cost 30 */
  CHECK_EQ(tower_repair_cost(&g, t), 6);                       /* ceil(9 * 2/3) */
  CHECK_EQ(tower_sell(&g, t), ACT_OK); CHECK_EQ(g.money, 135); CHECK_EQ(tower_at(&g, 2, 2), -1);
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

int main(void) {
  RUN(test_day_economy);
  return TEST_SUMMARY();
}
