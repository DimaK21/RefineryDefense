#include <string.h>
#include "economy.h"
#include "config.h"
#include "rng.h"

void game_new(Game *g, uint32_t seed) {
  memset(g, 0, sizeof(*g));
  g->rng = seed ? seed : 1;
  g->seed = rng_next(&g->rng);
  g->phase = PHASE_DAY;
  g->night = 1;
  g->money = START_MONEY;
  g->objects[OBJ_DISTILLATION].status = OBJ_ALIVE;
  g->objects[OBJ_DISTILLATION].hp = 100;
  g->objects[OBJ_TANK_A].status = OBJ_ALIVE;
  g->objects[OBJ_TANK_A].hp = 100;
}

int alive_objects(const Game *g) {
  int n = 0;
  for (int i = 0; i < OBJ_COUNT; i++)
    if (g->objects[i].status == OBJ_ALIVE) n++;
  return n;
}
